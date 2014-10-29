# -*- encoding: utf-8 -*-

import bottle
import bson
import datetime
import re
import time
import unicodedata
import urllib.parse

import elle.log
from .utils import api, require_logged_in
from . import regexp, error, transaction_status, notifier, invitation, cloud_buffer_token, mail
import uuid
import re
from pymongo import ASCENDING, DESCENDING
from .plugins.response import response

from infinit.oracles.meta.server.utils import json_value

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Transaction'

class Mixin:

  def is_sender(self, transaction, owner_id, device_id = None):
    assert isinstance(owner_id, bson.ObjectId)
    if transaction['sender_id'] != owner_id:
      return False
    if device_id is None:
      return True
    return transaction['sender_device_id'] == str(device_id)

  def is_recipient(self, transaction, owner_id, device_id = None):
    assert isinstance(owner_id, bson.ObjectId)
    if transaction['recipient_id'] != owner_id:
      return False
    if device_id is None:
      return True
    return transaction['recipient_device_id'] == str(device_id)

  def transaction(self, id, owner_id = None):
    assert isinstance(id, bson.ObjectId)
    transaction = self.database.transactions.find_one(id)
    if transaction is None:
      self.not_found('transaction %s doesn\'t exist' % id)
    if owner_id is not None:
      assert isinstance(owner_id, bson.ObjectId)
      if not owner_id in (transaction['sender_id'], transaction['recipient_id']):
        self.forbidden('transaction %s doesn\'t belong to you' % id)
    return transaction

  def cancel_transactions(self, user):
    for transaction in self.database.transactions.find(
      {
        '$or': [
          {'sender_id': user['_id']},
          {'recipient_id': user['_id']}
        ],
        'status': {'$nin': transaction_status.final}
      },
      fields = ['_id']):
      try:
        self._transaction_update(str(transaction['_id']),
                                 status = transaction_status.CANCELED,
                                 user = user)
      except error.Error as e:
        elle.log.warn('unable to cancel transaction (%s)' % \
                      str(transaction['_id']))
        continue

  @api('/transaction/by_hash/<transaction_hash>')
  def transaction_by_hash(self, transaction_hash):
    """
    Fetch transaction information corresponding to the given hash.

    transaction_hash -- Transaction hash for transaction.
    """
    with elle.log.debug('fetch transaction with hash: %s' % transaction_hash):
      transaction = self.database.transactions.find_one(
        {'transaction_hash': transaction_hash},
        fields = {
          '_id': False,
          'download_link': True,
          'files': True,
          'message': True,
          'recipient_id': True,
          'sender_fullname': True,
          'sender_id': True,
          'total_size': True,
        })
      if transaction is None:
        return self.not_found()
      else:
        return self.success(transaction)

  @api('/transaction/<id>')
  @require_logged_in
  def transaction_view(self, id: bson.ObjectId):
    return self.transaction(id, self.user['_id'])

  # FIXME: This is backward compatibility for /transaction/<id>.
  @api('/transaction/<id>/view')
  @require_logged_in
  def transaction_view_old(self, id: bson.ObjectId):
    assert isinstance(id, bson.ObjectId)
    try:
      transaction = self.transaction(id, self.user['_id'])
      return self.success(transaction)
    except error.Error as e:
      return self.fail(*e.args)

  @api('/transaction/create', method = 'POST')
  @require_logged_in
  def transaction_create(self,
                         id_or_email,
                         files,
                         files_count,
                         total_size,
                         is_directory,
                         device_id, # Can be determine by session.
                         message = ""):
    """
    Send a file to a specific user.
    If you pass an email and the user is not registered in infinit,
    create a 'ghost' in the database, waiting for him to register.

    id_or_email -- the recipient id or email.
    files -- the list of files names.
    files_count -- the number of files.
    total_size -- the total size.
    is_directory -- if the sent file is a directory.
    device_id -- the emiter device id.
    message -- an optional message.

    Errors:
    Using an id that doesn't exist.
    """
    with elle.log.trace("create transaction (recipient %s)" % id_or_email):
      user = self.user
      id_or_email = id_or_email.strip().lower()

      # if self.database.devices.find_one(bson.ObjectId(device_id)) is None:
      #   return self.fail(error.DEVICE_NOT_FOUND)

      new_user = False
      is_ghost = False
      invitee = 0
      peer_email = ""

      if re.match(regexp.Email, id_or_email): # email.
        elle.log.debug("%s is an email" % id_or_email)
        peer_email = id_or_email.lower().strip()
        # XXX: search email in each accounts.
        recipient = self.database.users.find_one({'email': peer_email})
        # if the user doesn't exist, create a ghost and invite.

        if not recipient:
          elle.log.trace("recipient unknown, create a ghost")
          new_user = True
          recipient_id = self._register(
            _id = self.database.users.save({}),
            email = peer_email,
            fullname = peer_email, # This is safe as long as we don't allow searching for ghost users.
            register_status = 'ghost',
            notifications = [],
            networks = [],
            swaggers = {},
            accounts = [{'type':'email', 'id':peer_email}],
            features = self.__roll_abtest(True)
          )
          recipient = self.database.users.find_one(recipient_id)
      else:
        try:
          recipient_id = bson.ObjectId(id_or_email)
        except Exception as e:
          return self.fail(error.USER_ID_NOT_VALID)
        recipient = self.database.users.find_one(recipient_id)

      if recipient is None:
        return self.fail(error.USER_ID_NOT_VALID)
      is_ghost = recipient['register_status'] == 'ghost'
      elle.log.debug("transaction recipient has id %s" % recipient['_id'])
      _id = user['_id']

      cloud_capable = self.user_version >= (0, 8, 11)
      elle.log.debug('Sender agent %s, version %s, cloud_capable %s, peer_new %s peer_ghost %s'
                     % (self.user_agent, self.user_version, cloud_capable, new_user,  is_ghost))
      transaction = {
        'sender_id': _id,
        'sender_fullname': user['fullname'],
        'sender_device_id': device_id, # bson.ObjectId(device_id),

        'recipient_id': recipient['_id'],
        'recipient_fullname': recipient['fullname'],

        # Empty until accepted.
        'recipient_device_id': '',
        'recipient_device_name': '',

        'message': message,

        'files': files,
        'files_count': files_count,
        'total_size': total_size,
        'is_directory': is_directory,

        'ctime': time.time(),
        'mtime': time.time(),
        'status': transaction_status.CREATED,
        'fallback_host': None,
        'fallback_port_ssl': None,
        'fallback_port_tcp': None,
        'aws_credentials': None,
        'is_ghost': is_ghost and cloud_capable,
        'strings': ' '.join([
              user['fullname'],
              user['handle'],
              user['email'],
              recipient['fullname'],
              recipient.get('handle', ""),
              recipient['email'],
              message,
              ] + files)
        }

      transaction_id = self.database.transactions.insert(transaction)
      self.__update_transaction_time(user)

      if not peer_email:
        peer_email = recipient['email']

      #FIXME : send invite email if initiator version will not attempt
      # ghost cloud upload
      if new_user and not cloud_capable:
        elle.log.debug("Client not cloud_capable, inviting now")
        invitation.invite_user(
          peer_email,
          mailer = self.mailer,
          mail_template = 'send-file',
          source = (user['fullname'], user['email']),
          database = self.database,
          merge_vars = {
            peer_email: {
              'filename': files[0],
              'note': message,
              'sendername': user['fullname'],
              'ghost_id': str(recipient.get('_id')),
              'sender_id': str(user['_id']),
              'avatar': self.user_avatar_route(recipient['_id']),
              'number_of_other_files': (files_count - 1),
            }}
        )
      if not new_user and not recipient.get('connected', False) and not is_ghost:
        elle.log.debug("recipient is disconnected")
        template_id = 'accept-file-only-offline'

        self.mailer.send_template(
          to = peer_email,
          template_name = template_id,
          reply_to = "%s <%s>" % (user['fullname'], user['email']),
          merge_vars = {
            peer_email: {
              'filename': files[0],
              'note': message,
              'sendername': user['fullname'],
              'avatar': self.user_avatar_route(recipient['_id']),
            }}
          )

      self._increase_swag(user['_id'], recipient['_id'])

      return self.success({
          'created_transaction_id': transaction_id,
          'remaining_invitations': user.get('remaining_invitations', 0),
          'recipient_is_ghost': is_ghost,
          })

  def __update_transaction_time(self, user):
    self.database.users.update(
      {'_id': user['_id']},
      {
        '$set':
        {
          'last_transaction.time': datetime.datetime.utcnow(),
        }
      })


  @api('/transactions')
  @require_logged_in
  def transactions(self,
                   filter : json_value = transaction_status.final + [transaction_status.CREATED],
                   negate : json_value = True,
                   peer_id : bson.ObjectId = None,
                   count : int = 100,
                   offset : int = 0,
                 ):
    with elle.log.trace("get %s transactions with%s status in %s" % \
                        (count, negate and "out" or "", filter)):
      user_id = self.user['_id']
      if peer_id is not None:
        query = {
          '$or':
          [
            { 'recipient_id': user_id, 'sender_id': peer_id, },
            { 'sender_id': user_id, 'recipient_id': peer_id, },
          ]}
      else:
        query = {
          '$or':
          [
            { 'sender_id': user_id },
            { 'recipient_id': user_id },
          ]
        }
      query['status'] = {'$%s' % (negate and 'nin' or 'in'): filter}
      res = self.database.transactions.aggregate([
        {'$match': query},
        {'$sort': {'mtime': -1}},
        {'$skip': offset},
        {'$limit': count},
      ])['result']
      return self.success({'transactions': res})

  # Previous (shitty) transactions fetching API that only returns ids.
  # This is for backwards compatability < 0.9.1.
  @api('/transactions', method = 'POST')
  @require_logged_in
  def transaction_post(self,
                       filter = transaction_status.final + [transaction_status.CREATED],
                       type = False,
                       peer_id = None,
                       count = 100,
                       offset = 0):
    return self._transactions(filter = filter,
                              peer_id = peer_id,
                              type = type,
                              count = count,
                              offset = offset)

  def _transactions(self,
                    filter,
                    type,
                    peer_id,
                    count,
                    offset):
    """
    Get all transaction involving user (as sender or recipient) which fit parameters.

    filter -- a list of transaction status.
    type -- make request inclusiv or exclusiv.
    count -- the number of transactions to get.
    offset -- the number of transactions to skip.
    _with -- The peer id if specified.
    """
    inclusive = type
    user_id = self.user['_id']

    if peer_id is not None:
      peer_id = bson.ObjectId(peer_id)
      query = {
        '$or':
        [
          { 'recipient_id': user_id, 'sender_id': peer_id, },
          { 'sender_id': user_id, 'recipient_id': peer_id, },
        ]}
    else:
      query = {
        '$or':
          [
            { 'sender_id': user_id },
            { 'recipient_id': user_id },
          ]
        }

    query['status'] = {'$%s' % (inclusive and 'in' or 'nin'): filter}

    from pymongo import ASCENDING, DESCENDING
    find_params = {
      'spec': query,
      'limit': count,
      'skip': offset,
      'fields': ['_id'],
      'sort': [
        ('mtime', DESCENDING),
        ],
      }

    return self.success(
      {
        "transactions": [ t['_id'] for t in self.database.transactions.find(**find_params)
                        ]
      }
    )

  def cloud_cleanup_transaction(self, transaction):
    # cloud_buffer_token.delete_directory(transaction.id)
    return {}

  def on_accept(self, transaction, user, device_id, device_name):
    with elle.log.trace("accept transaction as %s" % device_id):
      if device_id is None or device_name is None:
        self.bad_request()
      device_id = uuid.UUID(device_id)
      if str(device_id) not in self.user['devices']:
        raise error.Error(error.DEVICE_DOESNT_BELONG_TO_YOU)
      self.__update_transaction_time(user)
      return {
        'recipient_fullname': self.user['fullname'],
        'recipient_device_name' : device_name,
        'recipient_device_id': str(device_id)
      }

  def _hash_transaction(self, transaction):
    """
    Generate a unique hash for a transaction based on a random number and the
    transaction's ID.
    """
    import hashlib, random
    random.seed()
    random.random()
    string_to_hash = str(transaction['_id']) + str(random.random())
    txn_hash = hashlib.sha256(string_to_hash.encode('utf-8')).hexdigest()
    elle.log.debug('transaction (%s) hash: %s' % (transaction['_id'], txn_hash))
    return txn_hash

  def on_finished(self, transaction, device_id, device_name, user):
    elle.log.log('Transaction finished');
    # Guess if this was a ghost cloud upload or not
    recipient = self.database.users.find_one(transaction['recipient_id'])
    elle.log.log('Peer status: %s' % recipient['register_status'])
    elle.log.log('transaction: %s' % transaction.keys())
    if transaction.get('is_ghost', False):
      peer_email = recipient['email']
      transaction_id = transaction['_id']
      elle.log.trace("send invitation to new user %s for transaction %s" % (
        peer_email, transaction_id))
      # Figure out what the sender will ghost-upload
      # This heuristic must be in sync with the sender!
      ghost_upload_file = ''
      files = transaction['files']
      # FIXME: this name computation is duplicated from client !
      if len(files) == 1:
        if transaction['is_directory']:
          #C++ side is doing a replace_extension
          parts = files[0].split('.')
          if len(parts) == 1:
            ghost_upload_file = files[0] + '.zip'
          else:
            ghost_upload_file = '.'.join(parts[0:-1]) + '.zip'
        else:
          ghost_upload_file = files[0]
      else:
        ghost_upload_file = '%s files.zip' % len(files)
      # Generate GET URL for ghost cloud uploaded file
      # FIXME: AFAICT nothing prevent us from generating this directly
      # on transaction creation and greatly simplify the client and
      # server code.
      ghost_get_url = cloud_buffer_token.generate_get_url(
        self.aws_region, self.aws_buffer_bucket,
        transaction_id,
        ghost_upload_file)
      elle.log.log('Generating cloud GET URL for %s: %s'
        % (ghost_upload_file, ghost_get_url))
      # Generate hash for transaction and store it in the transaction
      # collection.
      transaction_hash = self._hash_transaction(transaction)
      mail_template = 'send-file-url'
      if 'features' in recipient and 'send_file_url_template' in recipient['features']:
        mail_template = recipient['features']['send_file_url_template']
      invitation.invite_user(
        peer_email,
        mailer = self.mailer,
        mail_template = mail_template,
        source = (user['fullname'], user['email']),
        database = self.database,
        merge_vars = {
          peer_email: {
            'filename': files[0],
            'sendername': user['fullname'],
            'sender_email': user['email'],
            'sender_avatar': 'https://%s/user/%s/avatar' %
              (bottle.request.urlparts[1], user['_id']),
            'note': transaction['message'],
            'transaction_hash': transaction_hash,
            'transaction_id': str(transaction['_id']),
            'number_of_other_files': len(files) - 1,
          }}
      )
      return {
        'transaction_hash': transaction_hash,
        'download_link': ghost_get_url,
      }
    else:
      elle.log.trace('Recipient is not a ghost, nothing to do')
      return {}

  @api('/transaction/update', method = 'POST')
  @require_logged_in
  def transaction_update(self,
                         transaction_id,
                         status,
                         device_id = None,
                         device_name = None):

    try:
      transaction_id = self._transaction_update(transaction_id,
                                                status,
                                                device_id,
                                                device_name,
                                                self.user)
    except error.Error as e:
      return self.fail(*e.args)

    return self.success({
      'updated_transaction_id': transaction_id,
    })

  def _transaction_update(self,
                          transaction_id,
                          status,
                          device_id = None,
                          device_name = None,
                          user = None):
    with elle.log.trace('update transaction %s to status %s' %
                        (transaction_id, status)):
      if user is None:
        user = self.user
      if user is None:
        response(404, {'reason': 'no such user'})
      # current_device is None if we do a delete user / reset account.
      if self.current_device is not None and device_id is None:
        device_id = str(self.current_device['id'])
      transaction_id = bson.ObjectId(transaction_id)
      transaction = self.transaction(transaction_id,
                                     owner_id = user['_id'])
      is_sender = self.is_sender(transaction, user['_id'], device_id)
      args = (transaction['status'], status)
      if transaction['status'] != status:
        allowed = transaction_status.transitions[transaction['status']][is_sender]
        if status not in allowed:
          fmt = 'changing status %s to %s not permitted'
          response(403, {'reason': fmt % args})
        if transaction['status'] in transaction_status.final:
          fmt = 'changing final status %s to %s not permitted'
          response(403, {'reason': fmt % args})
      diff = {}
      if status == transaction_status.ACCEPTED:
        diff.update(self.on_accept(transaction = transaction,
                                   user = user,
                                   device_id = device_id,
                                   device_name = device_name))
      elif status == transaction_status.FINISHED:
        diff.update(self.on_finished(transaction = transaction,
                                     device_id = device_id,
                                     device_name = device_name,
                                     user = user))
        if is_sender and transaction.get('is_ghost'):
          status = transaction_status.GHOST_UPLOADED
      elif status == transaction_status.CANCELED:
        if not transaction.get('canceler', None):
          diff.update({'canceler': {'user': user['_id'], 'device': device_id}})
          diff.update(self.cloud_cleanup_transaction(transaction = transaction))
      elif status in (transaction_status.FAILED,
                      transaction_status.REJECTED):
        diff.update(self.cloud_cleanup_transaction(
          transaction = transaction))
      diff.update({
        'status': status,
        'mtime': time.time()
      })
      # Don't update with an empty dictionary: it would empty the
      # object.
      if diff:
        transaction = self.database.transactions.find_and_modify(
          {'_id': transaction['_id']},
          {'$set': diff},
          new = True,
        )
        elle.log.debug("transaction updated")
        self.notifier.notify_some(
          notifier.PEER_TRANSACTION,
          recipient_ids = {transaction['sender_id'], transaction['recipient_id']},
          message = transaction,
        )
      return transaction_id

  @api('/transaction/search')
  @require_logged_in
  def transaction_search(self, text, limit, offset):
    text = ascii_string(text)
    query = {
      '$and':
      [
        {'strings': {'$regex': text,}},
        {
          '$or':
          [
            { 'recipient_id': self.user['_id'] },
            { 'sender_id': self.user['_id'] }
          ]
        }
      ]
    }
    find_params = {
      'limit': limit,
      'skip': offset,
      'fields': ['_id'],
      'sort': [('mtime', DESCENDING)],
    }
    return self.success({
        'transactions': list(
          t['_id'] for t in self.database.transactions.find(
            {'$query': query},
            **find_params
            )
          )
        })

  def __user_key(self, user_id, device_id):
    assert isinstance(user_id, bson.ObjectId)
    assert isinstance(device_id, uuid.UUID)
    return "%s-%s" % (str(user_id), str(device_id))

  def find_nodes(self, user_id, device_id):
      assert isinstance(user_id, bson.ObjectId)
      assert isinstance(device_id, uuid.UUID)
      return self.database.transactions.find(
        {
          "nodes.%s" % self.__user_key(user_id, device_id): {"$exists": True}
        })

  def update_node(self, transaction_id, user_id, device_id, node):
    with elle.log.trace("transaction %s: update node for device %s: %s" %
                         (transaction_id, device_id, node)):
      assert isinstance(transaction_id, bson.ObjectId)
      if node is None:
        operation = '$unset'
        value = 1
      else:
        operation = '$set'
        value = node
      return self.database.transactions.find_and_modify(
        {"_id": transaction_id},
        {operation: {"nodes.%s" % self.__user_key(user_id, device_id): value}},
        multi = False,
        new = True,
        )

  @api('/transaction/<transaction_id>/endpoints', method = 'PUT')
  @require_logged_in
  def put_endpoints(self,
                    transaction_id: bson.ObjectId,
                    device: uuid.UUID,
                    locals = [],
                    externals = []):
    """
    Connect the device to a transaction (setting ip and port).
    _id -- the id of the transaction.
    device -- the id of the device to link with.
    locals -- a set of local ip address and port.
    externals -- a set of externals ip address and port.
    """
    user = self.user
    transaction = self.transaction(transaction_id,
                                   owner_id = user['_id'])
    device_id = device
    device = self.device(id = str(device),
                         owner =  user['_id'])
    if str(device['id']) not in [transaction['sender_device_id'],
                                 transaction['recipient_device_id']]:
      elle.log.trace('Device mismatch: %s neither %s nor %s'
        % ( str(device['id']),
            transaction['sender_device_id'],
            transaction['recipient_device_id']))
      debug = {
        'current device': bottle.request.session.get('device'),
        'sent device': device,
        'transaction devices': [
          transaction['sender_device_id'],
          transaction['recipient_device_id'],
        ],
      }
      self.forbidden('transaction is not for this device: %r' % debug)
    node = dict()
    node['locals'] = [
      {'ip' : v['ip'], 'port' : v['port']}
      for v in locals or () if v['ip'] != '0.0.0.0'
    ]
    node['externals'] = [
      {'ip' : v['ip'], 'port' : v['port']}
      for v in externals or () if v['ip'] != '0.0.0.0'
    ]
    with elle.log.trace(
        'transaction %s: update node for device %s: %s' %
        (transaction, device, node)):
      transaction = self.database.transactions.find_and_modify(
        {'_id': transaction_id},
        {'$set': {
          'nodes.%s' % self.__user_key(user['_id'], device_id): node
        }},
        multi = False,
        new = True,
      )
    self.__notify_reachability(transaction)
    return self.success()

  def __notify_reachability(self, transaction):
    with elle.log.trace("notify reachability for transaction %s" % transaction['_id']):
      # The None check is required because of old transactions in the
      # database where the endpoints of disconnected users where set to
      # null instead of removed.
      if len(transaction['nodes']) == 2 and list(transaction['nodes'].values()).count(None) == 0:
        elle.log.trace("both nodes connected: %s" % transaction['nodes'])
        def notify(transaction, notified, other):
          key = self.__user_key(
            transaction['%s_id' % other],
            uuid.UUID(transaction['%s_device_id' % other]))
          endpoints = transaction['nodes'][key]
          destination = set((transaction['%s_device_id' % notified],))
          device_ids = (transaction['%s_device_id' % notified],
                        transaction['%s_device_id' % other])
          message = {
            'transaction_id': str(transaction['_id']),
            'peer_endpoints': endpoints,
            'devices': list(map(str, device_ids)),
            'status': True,
          }
          self.notifier.notify_some(
            notifier.PEER_CONNECTION_UPDATE,
            device_ids = destination,
            message = message,
          )
        notify(transaction, 'sender', 'recipient')
        notify(transaction, 'recipient', 'sender')
      else:
        elle.log.trace("only one node connected: %s" % transaction['nodes'])

  @api('/transaction/connect_device', method = "POST")
  @require_logged_in
  def connect_device(self,
                     _id: bson.ObjectId,
                     device_id: uuid.UUID, # Can be determined by session.
                     locals = [],
                     externals = []):
    """
    Connect the device to a transaction (setting ip and port).
    _id -- the id of the transaction.
    device_id -- the id of the device to link with.
    locals -- a set of local ip address and port.
    externals -- a set of externals ip address and port.
    """
    # XXX: We should check the state of the transaction.

    #regexp.Validator(regexp.ID, error.TRANSACTION_ID_NOT_VALID)
    #regexp.Validator(regexp.DeviceID, error.DEVICE_ID_NOT_VALID)
    user = self.user
    assert isinstance(_id, bson.ObjectId)
    assert isinstance(device_id, uuid.UUID)

    transaction = self.transaction(_id, owner_id = user['_id'])
    device = self.device(id = str(device_id),
                         owner =  user['_id'])
    if str(device_id) not in [transaction['sender_device_id'],
                              transaction['recipient_device_id']]:
      return self.fail(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

    node = dict()

    if locals is not None:
      # Generate a list of dictionary ip:port.
      # We can not take the local_addresses content directly:
      # it's not checked before this point. Therefor, it's insecure.
      node['locals'] = [
        {"ip" : v["ip"], "port" : v["port"]}
        for v in locals if v["ip"] != "0.0.0.0"
        ]
    else:
      node['locals'] = []

    if externals is not None:
      node['externals'] = [
        {"ip" : v["ip"], "port" : v["port"]}
        for v in externals if v["ip"] != "0.0.0.0"
        ]
    else:
      node['externals'] = []

    transaction = self.update_node(transaction_id = transaction['_id'],
                                   user_id = user['_id'],
                                   device_id = device_id,
                                   node = node)

    elle.log.trace("device %s connected to transaction %s as %s" % (device_id, _id, node))
    self.__notify_reachability(transaction)
    return self.success()

  @api('/transaction/<transaction_id>/endpoints', method = "POST")
  @require_logged_in
  def endpoints(self,
                transaction_id: bson.ObjectId,
                device_id: uuid.UUID, # Can be determined by session.
                self_device_id: uuid.UUID # Can be determined by session.
                ):
    """
    Return ip port for a selected node.
    device_id -- the id of the device to get ips.
    self_device_id -- the id of your device.
    """
    user = self.user

    transaction = self.transaction(transaction_id, owner_id = user['_id'])
    is_sender = self.is_sender(transaction, user['_id'], str(self.current_device['id']))

    # XXX: Ugly.
    if is_sender:
      self_key = self.__user_key(transaction['sender_id'], self_device_id)
      peer_key = self.__user_key(transaction['recipient_id'], device_id)
    else:
      self_key = self.__user_key(transaction['recipient_id'], self_device_id)
      peer_key = self.__user_key(transaction['sender_id'], device_id)

    if (not self_key in transaction['nodes'].keys()) or (not transaction['nodes'][self_key]):
      return self.fail(error.DEVICE_NOT_FOUND, "you are not not connected to this transaction")

    if (not peer_key in transaction['nodes'].keys()) or (not transaction['nodes'][peer_key]):
      return self.fail(error.DEVICE_NOT_FOUND, "This user is not connected to this transaction")

    res = dict();

    addrs = {'locals': list(), 'externals': list()}
    peer_node = transaction['nodes'][peer_key]

    for addr_kind in ['locals', 'externals']:
      for a in peer_node[addr_kind]:
        if a and a["ip"] and a["port"]:
          addrs[addr_kind].append(
            (a["ip"], str(a["port"])))

    res['externals'] = ["{}:{}".format(*a) for a in addrs['externals']]
    res['locals'] =  ["{}:{}".format(*a) for a in addrs['locals']]
    # XXX: Remove when apertus is ready.
    res['fallback'] = ["88.190.48.55:9899"]

    return self.success(res)

  @api('/transaction/<transaction_id>/cloud_buffer')
  @require_logged_in
  def cloud_buffer(self, transaction_id : bson.ObjectId,
                   force_regenerate : json_value = True):
    """
    Return AWS credentials giving the user permissions to PUT (sender) or GET
    (recipient) from the cloud buffer.
    """
    user = self.user
    transaction = self.transaction(transaction_id, owner_id = user['_id'])

    # Ensure transaction is not in a final state.
    if transaction['status'] in transaction_status.final:
      return self.gone('Transaction already finalized(%s)' % transaction['status'])

    res = None

    amazon_time_format = '%Y-%m-%dT%H-%M-%SZ'
    now = time.gmtime()
    current_time = time.strftime(amazon_time_format, now)

    if not force_regenerate:
      res = transaction.get('aws_credentials', None)
      if res is not None:
        elle.log.debug('cloud_buffer: returning from cache')
        res['current_time'] = current_time
        return self.success(res)
    elle.log.debug('Regenerating AWS token')
    # As long as those creds are transaction specific there is no risk
    # in letting the recipient have WRITE access. This will no longuer hold
    # if cloud data ever gets shared among transactions.
    token_maker = cloud_buffer_token.CloudBufferToken(
      user['_id'], transaction_id, 'ALL',
      aws_region = self.aws_region, bucket_name = self.aws_buffer_bucket)
    raw_creds = token_maker.generate_s3_token()

    if raw_creds == None:
      return self.fail(error.UNABLE_TO_GET_AWS_CREDENTIALS)

    # Only send back required credentials.
    credentials = dict()
    credentials['access_key_id']     = raw_creds['AccessKeyId']
    credentials['secret_access_key'] = raw_creds['SecretAccessKey']
    credentials['session_token']     = raw_creds['SessionToken']
    credentials['expiration']        = raw_creds['Expiration']
    credentials['protocol']          = 'aws'
    credentials['region']            = self.aws_region
    credentials['bucket']            = self.aws_buffer_bucket
    credentials['folder']            = transaction_id
    credentials['current_time']      = current_time

    elle.log.debug("Storing aws_credentials in DB")
    transaction.update({'aws_credentials': credentials})
    self.database.transactions.update(
      {'_id': transaction_id},
      {'$set': {'aws_credentials': credentials}})
    return self.success(credentials)
