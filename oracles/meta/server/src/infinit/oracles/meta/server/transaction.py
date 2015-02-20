# -*- encoding: utf-8 -*-

import bottle
import bson
import datetime
import re
import time
import unicodedata
import urllib.parse
import requests
import json

import elle.log
from .utils import \
  api, require_logged_in, require_logged_in_or_admin, require_key
from . import regexp, error, transaction_status, notifier, invitation, cloud_buffer_token, cloud_buffer_token_gcs, mail
import uuid
import re
from pymongo import ASCENDING, DESCENDING
from .plugins.response import response

from infinit.oracles.meta.server.utils import json_value

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Transaction'

# This create a code with more than 8 billions possibilities.
code_alphabet = '23456789abcdefghijkmnpqrstuvwxyz'
code_length = 7

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

  def change_transactions_recipient(self, current_owner, new_owner):
    # We can't do that as a batch because update won't give us the list
    # of updated transactions.
    while True:
      transaction = self.database.transactions.find_and_modify(
        {
          'recipient_id': current_owner['_id']
        },
        {
          '$set': {
            'recipient_id': new_owner['_id'],
            'modification_time': self.now,
            'mtime': time.time(),
          }
        },
        new = True)
      if transaction is None:
        return
      if transaction['status'] != transaction_status.CREATED:
        self.notifier.notify_some(
          notifier.PEER_TRANSACTION,
          recipient_ids = {transaction['sender_id'],
                           transaction['recipient_id']},
          message = transaction,
        )

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
        # FIXME: _transaction_update will re-perform a mongo search on
        # the transaction id ...
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

  @api('/transactions/<id>/downloaded', method='POST')
  @require_key
  def transaction_download_api(self, id: bson.ObjectId):
    return self.transaction_download(id)

  def transaction_download(self, id: bson.ObjectId):
    diff = {'status': transaction_status.FINISHED}
    transaction = self.database.transactions.find_and_modify(
      {'_id': id},
      {'$set': diff},
      new = False,
    )
    if transaction is None:
      self.not_found({
        'reason': 'transaction %s not found' % id,
        'transaction_id': id,
      })
    if transaction['status'] != transaction_status.FINISHED:
      self.__update_transaction_stats(
        transaction['recipient_id'],
        counts = ['received_ghost', 'received'],
        time = False)
      self.__update_transaction_stats(
        transaction['sender_id'],
        counts = ['reached_peer', 'reached'],
        time = False)
      self.notifier.notify_some(
        notifier.PEER_TRANSACTION,
        recipient_ids = {transaction['sender_id'],
                         transaction['recipient_id']},
        message = transaction,
      )
      return diff
    else:
      return {}

  @api('/transaction/<id>')
  def transaction_view(self, id: bson.ObjectId, key = None):
    if self.admin:
      return self.transaction(id, None)
    elif self.logged_in:
      return self.transaction(id, self.user['_id'])
    else:
      self.check_key(key)
      transaction = self.database.transactions.find_one(
        { '_id': id },
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
        self.not_found()
      else:
        return transaction

  @api('/transaction/create', method = 'POST')
  @require_logged_in
  def transaction_create_api(self,
                             id_or_email,
                             files,
                             files_count,
                             total_size,
                             is_directory,
                             device_id, # Can be determine by session.
                             message = ""):
    return self.transaction_create(
      self.user,
      id_or_email,
      files, files_count, total_size, is_directory,
      device_id,
      message)

  def transaction_create(self,
                         sender,
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
      id_or_email = id_or_email.strip().lower()

      new_user = False
      is_ghost = False
      invitee = 0
      peer_email = ""

      if re.match(regexp.Email, id_or_email): # email.
        elle.log.debug("%s is an email" % id_or_email)
        peer_email = id_or_email.lower().strip()
        # XXX: search email in each accounts.
        recipient = self.__user_fetch(
          {'accounts.id': peer_email},
          fields = self.__user_view_fields)
        # if the user doesn't exist, create a ghost and invite.

        if not recipient:
          elle.log.trace("recipient unknown, create a ghost")
          new_user = True
          features = self._roll_features(True)
          recipient_id = self._register(
            email = peer_email,
            fullname = peer_email, # This is safe as long as we don't allow searching for ghost users.
            register_status = 'ghost',
            notifications = [],
            networks = [],
            devices = [],
            swaggers = {},
            accounts = [{'type':'email', 'id':peer_email}],
            features = features
          )
          recipient = self.__user_fetch(
            recipient_id,
            fields = self.__user_view_fields + ['email'])
          # Post new_ghost event to metrics
          url = 'http://metrics.9.0.api.production.infinit.io/collections/users'
          metrics = {
            'event': 'new_ghost',
            'user': str(recipient['_id']),
            'features': features,
            'sender': str(sender['_id']),
            'timestamp': time.time(),
          }
          res = requests.post(
            url,
            headers = {'content-type': 'application/json'},
            data = json.dumps(metrics),
          )
          elle.log.debug('metrics answer: %s' % res)
      else:
        try:
          recipient_id = bson.ObjectId(id_or_email)
        except Exception as e:
          return self.fail(error.USER_ID_NOT_VALID)
        recipient = self.__user_fetch(
          recipient_id,
          fields = self.__user_view_fields + ['email'])

      if recipient is None:
        return self.fail(error.USER_ID_NOT_VALID)
      if recipient['register_status'] == 'merged':
        assert isinstance(recipient['merged_with'], bson.ObjectId)
        recipient = self.__user_fetch(
          {
            '_id': recipient['merged_with']
          },
          fields = self.__user_view_field
        )
        if recipient is None:
          return self.fail(error.USER_ID_NOT_VALID)
      if recipient['register_status'] == 'deleted':
        self.gone({
          'reason': 'user %s is deleted' % recipient['_id'],
          'recipient_id': recipient['_id'],
        })
      is_ghost = recipient['register_status'] == 'ghost'
      elle.log.debug("transaction recipient has id %s" % recipient['_id'])
      _id = sender['_id']

      elle.log.debug('Sender agent %s, version %s, peer_new %s peer_ghost %s'
                     % (self.user_agent, self.user_version, new_user,  is_ghost))
      transaction = {
        'sender_id': _id,
        'sender_fullname': sender['fullname'],
        'sender_device_id': device_id, # bson.ObjectId(device_id),

        'recipient_id': recipient['_id'],
        'recipient_fullname': recipient['fullname'],

        'involved': [_id, recipient['_id']],
        # Empty until accepted.
        'recipient_device_id': '',
        'recipient_device_name': '',

        'message': message,

        'files': files,
        'files_count': files_count,
        'total_size': total_size,
        'is_directory': is_directory,

        'creation_time': self.now,
        'modification_time': self.now,
        'ctime': time.time(),
        'mtime': time.time(),
        'status': transaction_status.CREATED,
        'fallback_host': None,
        'fallback_port_ssl': None,
        'fallback_port_tcp': None,
        'aws_credentials': None,
        'is_ghost': is_ghost,
        'strings': ' '.join([
              sender['fullname'],
              sender['handle'],
              self.user_identifier(sender),
              recipient['fullname'],
              recipient.get('handle', ""),
              self.user_identifier(recipient),
              message,
              ] + files)
        }

      transaction_id = self.database.transactions.insert(transaction)
      self.__update_transaction_stats(
        sender,
        counts = ['sent_peer', 'sent'],
        pending = transaction,
        time = True)

      if not peer_email:
        peer_email = recipient.get('email', None)

      if peer_email is not None:
        recipient_offline = all(d.get('trophonius') is None
                                for d in recipient.get('devices', []))
        if not new_user and recipient_offline and not is_ghost:
          elle.log.debug("recipient is disconnected")
          template_id = 'accept-file-only-offline'

          self.mailer.send_template(
            to = peer_email,
            template_name = template_id,
            reply_to = "%s <%s>" % (sender['fullname'], self.user_identifier(sender)),
            merge_vars = {
              peer_email: {
                'filename': files[0],
                'note': message,
                'sendername': sender['fullname'],
                'avatar': self.user_avatar_route(recipient['_id']),
              }}
            )

      self._increase_swag(sender['_id'], recipient['_id'])

      return self.success({
          'created_transaction_id': transaction_id,
          'remaining_invitations': sender.get('remaining_invitations', 0),
          'recipient_is_ghost': is_ghost,
          'recipient': self.__user_view(recipient),
        })

  def __update_transaction_stats(self,
                                 user,
                                 time = True,
                                 counts = None,
                                 pending = None):
    if isinstance(user, dict):
      user = user['_id']
    update = {}
    if time:
      update.setdefault('$set', {})
      update['$set']['last_transaction.time'] = self.now
      update['$set']['activated'] = True
    if counts is not None:
      update.setdefault('$inc', {})
      update['$inc'].update(
        dict(('transactions.%s' % field, 1) for field in counts))
    if pending is not None:
      update.setdefault('$set', {})
      update.setdefault('$push', {})
      update['$push']['transactions.pending'] = pending['_id']
      update['$set']['transactions.pending_has'] = True
    self.database.users.update({'_id': user}, update)

  def __complete_transaction_stats(self, user, transaction):
    if isinstance(user, dict):
      user = user['_id']
    res = self.database.users.update(
      {'_id': user},
      {'$pull': {'transactions.pending': transaction['_id']}},
    )
    if res['n']:
      self.database.users.update(
        {'_id': user, 'transactions.pending': []},
        {'$set': {'transactions.pending_has': False}},
      )

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
          '$and':
          [
            { 'involved': user_id},
            { 'involved': peer_id},
          ]}
      else:
        query = {
          'involved': user_id
        }
      query['status'] = {'$%s' % (negate and 'nin' or 'in'): filter}
      res = self.database.transactions.aggregate([
        {'$match': query},
        {'$sort': {'mtime': -1}},
        {'$skip': offset},
        {'$limit': count},
      ])['result']
      # FIXME: clients <= 0.9.22 don't start a PeerReceiveMachineFSM
      # on an unknown status. Make them think clould_buffered is the
      # same as created.
      for t in res:
        if t['status'] == transaction_status.CLOUD_BUFFERED:
          t['status'] = transaction_status.INITIALIZED
      # /FIXME
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
      if str(device_id) not in map(lambda x: x['id'], user['devices']):
        raise error.Error(error.DEVICE_DOESNT_BELONG_TO_YOU)
      self.__update_transaction_stats(
        user,
        counts = ['accepted_peer', 'accepted'],
        pending = transaction,
        time = True)
      res = {
        'recipient_fullname': user['fullname'],
        'recipient_device_name' : device_name,
        'recipient_device_id': str(device_id)
      }
      # As a recipient, if the aws credential are empty, don't fetch them.
      # It will be done in time and space if needed.
      credentials = transaction.get('aws_credentials', None)
      if credentials:
        res.update({
          'credentials': credentials
        })
      return res

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

  def on_ghost_uploaded(self, transaction, device_id, device_name, user):
    elle.log.log('Transaction finished');
    # Guess if this was a ghost cloud upload or not
    recipient = self.__user_fetch(transaction['recipient_id'],
                                  fields = self.__user_view_fields + ['email'])
    if recipient['register_status'] == 'deleted':
      self.gone({
        'reason': 'user %s is deleted' % recipient['_id'],
        'recipient_id': recipient['_id'],
      })
    elle.log.log('Peer status: %s' % recipient['register_status'])
    elle.log.log('transaction: %s' % transaction.keys())
    if transaction.get('is_ghost', False):
      peer_email = recipient['email']
      transaction_id = transaction['_id']
      elle.log.trace("send invitation to new user %s for transaction %s" % (
        peer_email, transaction_id))
      ghost_upload_file = self._upload_file_name(transaction)
      # Generate GET URL for ghost cloud uploaded file
      # FIXME: AFAICT nothing prevent us from generating this directly
      # on transaction creation and greatly simplify the client and
      # server code.
      # Figure out which backend was used
      backend_name = transaction['aws_credentials']['protocol']
      if backend_name == 'aws':
        backend = cloud_buffer_token
        bucket = self.aws_buffer_bucket
        region = self.aws_region
      elif backend_name == 'gcs':
        backend = cloud_buffer_token_gcs
        bucket = self.gcs_buffer_bucket
        region = self.gcs_region
      else:
        elle.log.err('unknown backend %s' % (backend_name))
      ghost_get_url = backend.generate_get_url(
        region, bucket,
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

      source = (user['fullname'], self.user_identifier(user))
      invitation.invite_user(
        peer_email,
        mailer = self.mailer,
        mail_template = mail_template,
        source = source,
        database = self.database,
        merge_vars = {
          peer_email: {
            'filename': transaction['files'][0],
            'recipient_email': recipient['email'],
            'recipient_name': recipient['fullname'],
            'sendername': user['fullname'],
            'sender_email': user.get('email', ''),
            'sender_avatar': 'https://%s/user/%s/avatar' %
              (bottle.request.urlparts[1], user['_id']),
            'note': transaction['message'],
            'transaction_hash': transaction_hash,
            'transaction_id': str(transaction['_id']),
            'number_of_other_files': len(transaction['files']) - 1,
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
      res = self._transaction_update(transaction_id,
                                     status,
                                     device_id,
                                     device_name,
                                     self.user)
    except error.Error as e:
      return self.fail(*e.args)

    res.update({
      'updated_transaction_id': transaction_id,
    })
    return self.success(res)

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
      if device_id is None and self.current_device is not None:
        device_id = str(self.current_device['id'])
      transaction_id = bson.ObjectId(transaction_id)
      transaction = self.transaction(transaction_id,
                                     owner_id = user['_id'])
      is_sender = self.is_sender(transaction, user['_id'], device_id)
      args = (transaction['status'], status)
      if transaction['status'] == status:
        return {}
      allowed = transaction_status.transitions[transaction['status']][is_sender]
      if status not in allowed:
        fmt = 'changing status %s to %s not permitted'
        response(403, {'reason': fmt % args})
      if transaction['status'] in transaction_status.final:
        fmt = 'changing final status %s to %s not permitted'
        response(403, {'reason': fmt % args})
      diff = {}
      operation = {}
      if status == transaction_status.ACCEPTED:
        diff.update(self.on_accept(transaction = transaction,
                                   user = user,
                                   device_id = device_id,
                                   device_name = device_name))
      elif status == transaction_status.GHOST_UPLOADED:
        diff.update(self.on_ghost_uploaded(transaction = transaction,
                                     device_id = device_id,
                                     device_name = device_name,
                                     user = user))
      elif status == transaction_status.CANCELED:
        if not transaction.get('canceler', None):
          diff.update({'canceler': {'user': user['_id'], 'device': device_id}})
          diff.update(self.cloud_cleanup_transaction(transaction = transaction))
      elif status in (transaction_status.FAILED,
                      transaction_status.REJECTED):
        diff.update(self.cloud_cleanup_transaction(
          transaction = transaction))
      elif status == transaction_status.FINISHED:
        self.__update_transaction_stats(
          transaction['recipient_id'],
          counts = ['received_peer', 'received'],
          time = True)
        self.__update_transaction_stats(
          transaction['sender_id'],
          counts = ['reached_peer', 'reached'],
          time = False)
      if status in transaction_status.final:
        operation["$unset"] = {"nodes": 1}
      # Don't override accepted with cloud_buffered.
      if status == transaction_status.CLOUD_BUFFERED and \
         transaction['status'] == transaction_status.ACCEPTED:
        diff.update({'status': transaction_status.ACCEPTED})
      else:
        diff.update({'status': status})
      diff.update({
        'mtime': time.time(),
        'modification_time': self.now,
      })
      # Don't update with an empty dictionary: it would empty the
      # object.
      if diff:
        operation["$set"] = diff
        if status in transaction_status.final:
          for i in ['recipient_id', 'sender_id']:
            self.__complete_transaction_stats(transaction[i],
                                              transaction)
        elif status in [transaction_status.statuses['ghost_uploaded'],
                        transaction_status.statuses['cloud_buffered']]:
          self.__complete_transaction_stats(transaction['sender_id'],
                                            transaction)
        transaction = self.database.transactions.find_and_modify(
          {'_id': transaction['_id']},
          operation,
          new = True,
        )
        elle.log.debug("transaction updated")
        self.notifier.notify_some(
          notifier.PEER_TRANSACTION,
          recipient_ids = {transaction['sender_id'], transaction['recipient_id']},
          message = transaction,
        )
      return diff

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
          '$or': [ # this optimizes the request by preventing a full table scan
            {'sender_device_id': str(device_id), 'sender_id': user_id},
            {'recipient_device_id': str(device_id), 'recipient_id': user_id},
          ],
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

  def _upload_file_name(self, transaction):
    """
    Return the name of the uploaded file from transaction data
    """
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
    return ghost_upload_file

  @api('/transaction/<transaction_id>/cloud_buffer')
  @require_logged_in
  def cloud_buffer(self, transaction_id : bson.ObjectId,
                   force_regenerate : json_value = True):
    return self._cloud_buffer(transaction_id, self.user,
                              force_regenerate)

  def _cloud_buffer(self, transaction_id, user, force_regenerate = True):
    """
    Return AWS credentials giving the user permissions to PUT (sender) or GET
    (recipient) from the cloud buffer.
    """
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
    if transaction['is_ghost'] and self.user_gcs_enabled:
      ghost_upload_file = self._upload_file_name(transaction)
      token_maker = cloud_buffer_token_gcs.CloudBufferTokenGCS(
        transaction_id, ghost_upload_file, self.gcs_buffer_bucket)
      ul = token_maker.get_upload_token()
      credentials = dict()
      credentials['protocol'] = 'gcs'
      credentials['url'] = ul
      credentials['expiration']        = (datetime.date.today()+datetime.timedelta(days=7)).isoformat()
      credentials['current_time']      = current_time
    else:
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

  def _user_transactions(self,
                         modification_time = None,
                         limit = 100):
    user_id = self.user['_id']
    query = {
       'involved': user_id
    }
    # XXX: Fix race condition!
    # If the transaction is updated between the 2 calls, it will be in both
    # lists.
    # The problem is that we want ALL the non finished transactions and the n
    # most recent transactions.

    # First, get the running transactions (no limit).
    query.update({
      'status': {'$nin': transaction_status.final + [transaction_status.CREATED] }
      })
    runnings = self.database.transactions.aggregate([
        {'$match': query},
        {'$sort': {'modification_time': DESCENDING}},
      ])['result']

    # Then get the 100 most recent transactions.
    query.update({
      'status': {'$in': transaction_status.final}
      })
    if modification_time:
      query.update({'modification_time': {'$gt': modification_time}})
    finals = self.database.transactions.aggregate([
        {'$match': query},
        {'$sort': {'modification_time': DESCENDING}},
        {'$limit': limit},
      ])['result']
    return {
      "running_transactions": list(runnings),
      "final_transactions": list(finals),
    }
