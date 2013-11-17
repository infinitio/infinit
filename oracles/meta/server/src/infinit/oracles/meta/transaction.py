# -*- encoding: utf-8 -*-


import bson
import re
import time
import unicodedata

import elle.log
from .utils import api, require_logged_in
from . import regexp, error, transaction_status, notifier, invitation
import uuid
import re
from pymongo import ASCENDING, DESCENDING

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Transaction'

class Mixin:

  def is_sender(self, transaction, owner_id):
    assert isinstance(owner_id, bson.ObjectId)
    return transaction['sender_id'] == owner_id

  def transaction(self, id, owner_id = None):
    assert isinstance(id, bson.ObjectId)
    transaction = self.database.transactions.find_one(id)
    if transaction is None:
      raise error.Error(error.TRANSACTION_DOESNT_EXIST)
    if owner_id is not None:
      assert isinstance(owner_id, bson.ObjectId)
      if not owner_id in (transaction['sender_id'], transaction['recipient_id']):
        raise error.Error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)
    return transaction

  @require_logged_in
  @api('/transaction/<id>/view')
  def transaction_view(self, id: bson.ObjectId):
    assert isinstance(id, bson.ObjectId)
    try:
      transaction = self.transaction(id, self.user['_id'])
      return self.success(transaction)
    except error.Error as e:
      return self.fail(*e.args)

  @require_logged_in
  @api('/transaction/create', method = 'POST')
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
    If you pass an email and the user is not registred in infinit,
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
    user = self.user
    id_or_email = id_or_email.strip()

    # if self.database.devices.find_one(bson.ObjectId(device_id)) is None:
    #   return self.fail(error.DEVICE_NOT_FOUND)

    new_user = False
    is_ghost = False
    invitee = 0
    invitee_email = ""

    if re.match(regexp.Email, id_or_email): # email.
      invitee_email = id_or_email
      # XXX: search email in each accounts.
      recipient = self.database.users.find_one({'email': id_or_email})
      # if the user doesn't exist, create a ghost and invite.

      if not recipient:
        new_user = True
        recipient_id = self._register(
          _id = self.database.users.save({}),
          email = invitee_email,
          fullname = invitee_email[0:invitee_email.index('@')],
          register_status = 'ghost',
          notifications = [],
          networks = [],
          swaggers = {},
          accounts=[{'type':'email', 'id':invitee_email}]
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

    _id = user['_id']

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
      'fallback': None,
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

    recipient_connected = recipient.get('connected', False)
    if not recipient_connected:
      if not invitee_email:
        invitee_email = recipient['email']

      if new_user:
        invitation.invite_user(
          invitee_email,
          mailer = self.mailer,
          mail_template = 'send-file',
          source = (user['fullname'], user['email']),
          filename = files[0],
          sendername = user['fullname'],
          database = self.database
        )

    self._increase_swag(user['_id'], recipient['_id'])

    return self.success({
        'created_transaction_id': transaction_id,
        'remaining_invitations': user.get('remaining_invitations', 0),
        })

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

  @require_logged_in
  @api('/transactions')
  def transcations_get(self,
                       filter = transaction_status.final + [transaction_status.CREATED],
                       type = False,
                       peer_id = None,
                       count = 100,
                       offset = 0):
    return self._transactions(filter = filter,
                              type = type,
                              peer_id = peer_id,
                              count = count,
                              offset = offset)

  @require_logged_in
  @api('/transactions', method = 'POST')
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

  def on_accept(self, transaction, device_id, device_name):
    with elle.log.trace("accept transaction as %s" % device_id):
      if device_id is None or device_name is None:
        self.bad_request()
      device_id = uuid.UUID(device_id)
      if str(device_id) not in self.user['devices']:
        raise error.Error(error.DEVICE_DOESNT_BELONG_TOU_YOU)

      transaction.update({
        'recipient_fullname': self.user['fullname'],
        'recipient_device_name' : device_name,
        'recipient_device_id': str(device_id),
      })

  @require_logged_in
  @api('/transaction/update', method = 'POST')
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
    with elle.log.log("update transaction %s: %s" %
                      (transaction_id, status)):
      if user is None:
        user = self.user
      if user is None:
        raise error.Error(error.UNKNOWN_USER)
      transaction = self.transaction(bson.ObjectId(transaction_id), owner_id = user['_id'])
      elle.log.debug("transaction: %s" % transaction)
      is_sender = self.is_sender(transaction, user['_id'])
      elle.log.debug("%s" % is_sender and "sender" or "recipient")
      if status not in transaction_status.transitions[transaction['status']][is_sender]:
        raise error.Error(
          error.TRANSACTION_OPERATION_NOT_PERMITTED,
          "Cannot change status from %s to %s (not permitted)." % (transaction['status'], status)
        )

      if transaction['status'] == status:
        raise error.Error(
          error.TRANSACTION_ALREADY_HAS_THIS_STATUS,
          "Cannont change status from %s to %s (same status)." % (transaction['status'], status))

      if transaction['status'] in transaction_status.final:
        raise error.Error(
          error.TRANSACTION_ALREADY_FINALIZED,
          "Cannot change status from %s to %s (already finalized)." % (transaction['status'], status)
          )

      from functools import partial

      callbacks = {
        transaction_status.INITIALIZED: None,
        transaction_status.ACCEPTED: partial(self.on_accept,
                                             transaction = transaction,
                                             device_id = device_id,
                                             device_name = device_name),
        transaction_status.FINISHED: None,
        transaction_status.CANCELED: None,
        transaction_status.FAILED: None,
        transaction_status.REJECTED: None
      }

      cb = callbacks[status]
      if cb is not None:
        cb()

      transaction['status'] = status
      transaction['mtime'] = time.time()
      self.database.transactions.save(transaction)

      elle.log.debug("transaction updated")

      self.notifier.notify_some(
        notifier.TRANSACTION,
        recipient_ids = {transaction['sender_id'], transaction['recipient_id']},
        message = transaction,
      )
      return transaction_id

  @require_logged_in
  @api('/transaction/search')
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
      return self.database.transactions.find_and_modify(
        {"_id": transaction_id},
        {"$set": {"nodes.%s" % self.__user_key(user_id, device_id): node}},
        multi = False,
        new = True,
        )

  @require_logged_in
  @api('/transaction/connect_device', method = "POST")
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

    node['fallback'] = []

    transaction = self.update_node(transaction_id = transaction['_id'],
                                   user_id = user['_id'],
                                   device_id = device_id,
                                   node = node)

    elle.log.trace("device %s connected to transaction %s as %s" % (device_id, _id, node))

    if len(transaction['nodes']) == 2 and list(transaction['nodes'].values()).count(None) == 0:
      devices_ids = {uuid.UUID(transaction['sender_device_id']),
                     uuid.UUID(transaction['recipient_device_id'])}
      self.notifier.notify_some(
        notifier.PEER_CONNECTION_UPDATE,
        recipient_ids = {transaction[k] for k in ['sender_id', 'recipient_id']},
        message = {
          "transaction_id": str(_id),
          "devices": list(map(str, devices_ids)),
          "status": True
        },
      )

    return self.success()

  @require_logged_in
  @api('/transaction/<transaction_id>/endpoints', method = "POST")
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
    is_sender = self.is_sender(transaction, user['_id'])

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

    addrs = {'locals': list(), 'externals': list(), 'fallback' : list()}
    peer_node = transaction['nodes'][peer_key]

    for addr_kind in ['locals', 'externals', 'fallback']:
      for a in peer_node[addr_kind]:
        if a and a["ip"] and a["port"]:
          addrs[addr_kind].append(
            (a["ip"], str(a["port"])))

    res['externals'] = ["{}:{}".format(*a) for a in addrs['externals']]
    res['locals'] =  ["{}:{}".format(*a) for a in addrs['locals']]
    # XXX: Remove when apertus is ready.
    res['fallback'] = ["88.190.48.55:9899"]

    return self.success(res)
