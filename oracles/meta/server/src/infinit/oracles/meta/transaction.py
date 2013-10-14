# -*- encoding: utf-8 -*-

import time
import unicodedata

from bson import ObjectId

from .utils import api, require_logged_in
from . import regexp, error, transaction_status, notifier

import re

class Mixin:

  @api('/transaction/<id>/view')
  @require_logged_in
  def transaction_view(self, id):
    transaction = self.database.transactions.find_one(ObjectId(id))
    if not transaction:
      return self.error(error.TRANSACTION_DOESNT_EXIST)
    if not self.user['_id'] in (transaction['sender_id'], transaction['recipient_id']):
      return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)
    return self.success(transaction)

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
    id_or_email = id_or_email.strip()

    # if self.database.devices.find_one(ObjectId(device_id)) is None:
    #   return self.error(error.DEVICE_NOT_FOUND)

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
        recipient = self._register(
          _id = recipient_id,
          email = invitee_email,
          fullname = invitee_email[0:invitee_email.index('@')],
          register_status = 'ghost',
          notifications = [],
          networks = [],
          swaggers = {},
          accounts=[{'type':'email', 'id':invitee_email}]
          )
    else:
      try:
        recipient_id = ObjectId(id_or_email)
      except Exception as e:
        return self.error(error.USER_ID_NOT_VALID)
      recipient = self.database.users.find_one(recipient_id)
      if recipient is None:
        return self.error(error.USER_ID_NOT_VALID)

    _id = self.user['_id']

    transaction = {
      'sender_id': _id,
      'sender_fullname': self.user['fullname'],
      'sender_device_id': device_id, # ObjectId(device_id),

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
      'strings': ' '.join([
            self.user['fullname'],
            self.user['handle'],
            self.user['email'],
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
        meta.invitation.invite_user(
          invitee_email,
          mailer = self.mailer,
          source = self.user['_id'],
          mail_template = 'send-file',
          reply_to = self.user['email'],
          filename = files[0],
          sendername = self.user['fullname'],
          user_id = str(self.user['_id']),
          database = self.database
        )

    self._increase_swag(self.user['_id'], recipient['_id'])

    return self.success({
        'created_transaction_id': transaction_id,
        'remaining_invitations': self.user.get('remaining_invitations', 0),
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
      peer_id = ObjectId(peer_id)
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

    print(query)

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

    print(find_params)

    return self.success(
      {
        "transactions": [ t['_id'] for t in self.database.transactions.find(**find_params)
                        ]
      }
    )

  @api('/transactions')
  @require_logged_in
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


  def _transaction_by_id(self, id, ensure_existence = True):
    assert isinstance(id, ObjectId)
    transaction = self.database.transactions.find_one(id)
    if transaction is None and ensure_existence:
      return self.error(error.TRANSACTION_DOESNT_EXIST)
    return transaction

  def validate_ownership(self, transaction):
    # Check that user has rights on the transaction
    is_sender = self.user['_id'] == transaction['sender_id']
    is_receiver = self.user['_id'] == transaction['recipient_id']
    if not (is_sender or is_receiver):
      return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)
    return is_sender

  def on_accept(self, transaction):
    if 'device_id' not in self.data or 'device_name' not in self.data:
      return self.error(
        error.TRANSACTION_OPERATION_NOT_PERMITTED,
        "'device_id' or 'device_name' key is missing")

    device_id = database.ObjectId(self.data["device_id"])
    if device_id not in self.user['devices']:
      return self.error(error.DEVICE_NOT_VALID)

    transaction.update({
      'recipient_fullname': self.user['fullname'],
      'recipient_device_name' : self.data['device_name'],
      'recipient_device_id': device_id,
    })

  def _update_status(self, transaction, status, is_sender):
    if transaction['status'] in transaction_status.final:
      return self.error(
        error.TRANSACTION_ALREADY_FINALIZED,
        "Cannot change status from %s to %s." % (transaction['status'], status)
        )

    if transaction['status'] == status:
      return self.error(
        error.TRANSACTION_ALREADY_HAS_THIS_STATUS,
        "Cannont change status from %s to %s." % (transaction['status'], status))


    if status not in transaction_status.transitions[transaction['status']][is_sender]:
      return self.error(
        error.TRANSACTION_OPERATION_NOT_PERMITTED,
        "Cannot change status from %s to %s." % (transaction['status'], self.data['status'])
        )

    from functools import partial

    callbacks = {
      transaction_status.INITIALIZED: None,
      transaction_status.ACCEPTED: partial(self.on_accept, transaction),
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

    device_ids = [transaction['sender_device_id']]
    recipient_ids = None
    if isinstance(transaction['recipient_device_id'], ObjectId):
      device_ids.append(transaction['recipient_device_id'])
    else:
      recipient_ids = [transaction['recipient_id']]

    self.notifier.notify_some(
      notifier.TRANSACTION,
      device_ids = device_ids,
      recipient_ids = recipient_ids,
      message = transaction,
    )

  @api('/transaction/update', method = 'POST')
  @require_logged_in
  def transaction_update(self, transaction_id, status):
    transaction = self._transaction_by_id(ObjectId(transaction_id))
    is_sender = self.validate_ownership(transaction)

    self._update_status(transaction, status, is_sender)

    return self.success({
      'updated_transaction_id': transaction_id,
    })

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
      'sort': [('mtime', -1)],
    }
    return self.success({
        'transactions': list(
          t['_id'] for t in self.database.transactions.find(
            {'$query': query},
            **find_params
            )
          )
        })

  @api('/transaction/connect_device', method = "POST")
  @require_logged_in
  def connect_device(self, _id, device_id, locals = [], externals = []):
    """
    Connect the device to a transaction (setting ip and port).
    _id -- the id of the transaction.
    device_id -- the id of the device to link with.
    locals -- a set of local ip address and port.
    externals -- a set of externals ip address and port.
    """

    #regexp.Validator(regexp.ID, error.TRANSACTION_ID_NOT_VALID)
    #regexp.Validator(regexp.DeviceID, error.DEVICE_ID_NOT_VALID)

    transaction_id = ObjectId(_id)

    transaction = self.database.transactions.find_one(transaction_id)
    if transaction is None:
      self.fail(error.TRANSACTION_DOESNT_EXIST)

    device_id = ObjectId(device_id)
    device = self.database.devices.find_one(device_id)
    if not device:
      return self.error(error.DEVICE_NOT_FOUND)

    if device_id not in [transaction['sender_device_id'],
                         transaction['recipient_device_id']]:
      return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

    node = dict()

    if locals is not None:
      # Generate a list of dictionary ip:port.
      # We can not take the local_addresses content directly:
      # it's not checked before this point. Therefor, it's insecure.
      node['locals'] = [
        {"ip" : v["ip"], "port" : v["port"]}
        for v in local_addresses if v["ip"] != "0.0.0.0"
        ]
    else:
      node['locals'] = []

    if externals is not None:
      node['externals'] = [
        {"ip" : v["ip"], "port" : v["port"]}
        for v in external_addresses if v["ip"] != "0.0.0.0"
        ]
    else:
      node['externals'] = []

    node['fallback'] = []

    transaction = self.database.transactions.find_and_modify(
      {"_id": transaction_id},
      {"$set": {"nodes.%s" % (str(device_id),): node}},
      multi = False
    )

    print("device %s connected to transaction %s as %s" % (device_id, transaction_id, node))

    if len(transaction['nodes']) == 2 and list(transaction['nodes'].values()).count(None) == 0:
      self.notifier.notify_some(
        notifier.PEER_CONNECTION_UPDATE,
        device_ids = list(transaction['nodes'].keys()),
        message = {
          "transaction_id": str(transaction_id),
          "devices": list(transaction['nodes'].keys()),
          "status": True
        },
      )

    return self.success({})

  @api('/transaction/<transaction_id>/endpoints', method = "POST")
  @require_logged_in
  def endpoints(self, transaction_id, device_id, self_device_id):
    """
    Return ip port for a selected node.
    device_id -- the id of the device to get ips.
    self_device_id -- the id of your device.
    """

    transaction = self.database.transactions.find_one(ObjectId(transaction_id))
    if not transaction:
      return self.error(error.TRANSACTION_DOESNT_EXIST)

    if self.user['_id'] not in [transaction['sender_id'], transaction['recipient_id']]:
      return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

    if (not self_device_id in transaction['nodes'].keys()) or (not transaction['nodes'][self_device_id]):
      return self.error(error.DEVICE_NOT_FOUND, "you are not not connected to this transaction")

    if (not device_id in transaction['nodes'].keys()) or (not transaction['nodes'][device_id]):
      return self.error(error.DEVICE_NOT_FOUND, "This user is not connected to this transaction")

    res = dict();

    addrs = {'locals': list(), 'externals': list(), 'fallback' : list()}
    user_node = transaction['nodes'][device_id];

    for addr_kind in ['locals', 'externals', 'fallback']:
        for a in user_node[addr_kind]:
            if a and a["ip"] and a["port"]:
                addrs[addr_kind].append(
                    (a["ip"], str(a["port"])))

    print("addrs is: ", addrs)

    res['externals'] = ["{}:{}".format(*a) for a in addrs['externals']]
    res['locals'] =  ["{}:{}".format(*a) for a in addrs['locals']]
    res['fallback'] = ["{}:{}".format(*a) for a in self.__application__.fallback]

    return self.success(res)
