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
from .utils import *
from . import regexp, error, transaction_status, notifier, invitation, cloud_buffer_token, cloud_buffer_token_gcs, mail, utils
import uuid
import re
from pymongo import ASCENDING, DESCENDING
from .plugins.response import response

from infinit.oracles.meta.server.utils import json_value
import infinit.oracles.emailer

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

    # handle both negative search and empty transaction
    if not transaction or len(transaction) == 1:
      self.not_found('transaction %s doesn\'t exist' % id)
    if owner_id is not None:
      assert isinstance(owner_id, bson.ObjectId)
      if not owner_id in (transaction['sender_id'], transaction['recipient_id']):
        self.forbidden('transaction %s doesn\'t belong to you' % id)
    return transaction

  def change_transactions_recipient(self, current_owner, new_owner):
    # XXX:
    # 1: We can't do that as a batch because update won't give us the list
    # of updated transactions to send notifications.
    # 2: Also, involved will contain both new and old recipient id.
    #
    # 1: could be achived by doing a 'resynch' notification, forcing the user
    # to resynch in model.
    # 2: could be done in two steps, in order to get the sender id and reforge
    # a complete 'involved' field.
    #
    # Let's consider that most of the ghosts don't have that many
    # transactions...
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
          },
          # Cannot pull the old one and add the new one at the same time.
          # (read 2.).
          # '$pull': {
          #   'involved': current_owner['_id']
          # },
          '$addToSet': {
            'involved': new_owner['_id']
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

  def cancel_transactions(self, user, device_id = None):
    query = {
      'status': {'$nin': transaction_status.final},
      '$or': [
        {'sender_id': user['_id']},
        {'recipient_id': user['_id']}
      ]
    }
    if device_id is not None:
      assert 'sender_id' in query['$or'][0]
      assert 'recipient_id' in query['$or'][1]
      query['$or'][0].update({'sender_device_id': device_id})
      query['$or'][1].update({'recipient_device_id': device_id})
    for transaction in self.database.transactions.find(
        query,
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

  def __transaction_hash_fields(self, include_id = False):
    res = {
      '_id': False,
      'download_link': True,
      'files': True,
      'message': True,
      'recipient_id': True,
      'sender_fullname': True,
      'sender_id': True,
      'total_size': True,
      'paused': True,
    }
    if include_id:
      res['_id'] = True
    return res

  @api('/transaction/by_hash/<transaction_hash>')
  def transaction_by_hash(self, transaction_hash):
    """
    Fetch transaction information corresponding to the given hash.

    transaction_hash -- Transaction hash for transaction.
    """
    with elle.log.debug('fetch transaction with hash: %s' % transaction_hash):
      transaction = self.database.transactions.find_one(
        {'transaction_hash': transaction_hash},
        fields = self.__transaction_hash_fields()
      )
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
    self.__complete_transaction_pending_stats(
      transaction['recipient_id'], transaction)
    # handle both negative search and empty transaction
    if not transaction or len(transaction) == 1:
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
          'paused': True,
        })
      # handle both negative search and empty transaction
      if not transaction:
        self.not_found()
      else:
        return transaction

  @api('/transaction/create_empty', method='POST')
  @require_logged_in
  def transaction_create_empty_api(self):
    """
    Create an empty transaction, to be filled in a separate API call.
    This allows for the client finer snapshot granularity, along with easier
    cleanup of unfulfilled transactions.
    Deprecated by /transactions POST.

    Return: the newly created transaction id.
    """
    return self.transaction_create_empty()


  def transaction_create_empty(self):
    transaction_id = self.database.transactions.insert({})
    return {
      'created_transaction_id': transaction_id,
      }

  @api('/transaction/<t_id>', method='PUT')
  @require_logged_in
  def transaction_fill(self,
                       t_id: bson.ObjectId,
                       files,
                       files_count,
                       total_size,
                       is_directory,
                       device_id, # Can be determine by session.
                       recipient_device_id = None,
                       id_or_email = None,
                       recipient_identifier: utils.identifier = None,
                       message = ""):
    return self.transaction_create(
      sender = self.user,
      files = files,
      files_count = files_count,
      total_size = total_size,
      is_directory = is_directory,
      device_id = device_id,
      transaction_id = t_id,
      id_or_email = id_or_email,
      recipient_device_id = recipient_device_id,
      recipient_identifier = recipient_identifier,
      message = message)

  @api('/transactions', method = 'POST')
  @require_logged_in
  def transaction_post(self,
                       recipient_identifier: utils.identifier,
                       files,
                       files_count,
                       message):
    """
    Create a bare transaction, with minimal information and placeholder values.
    Deprecates /transactions/create_empty POST.
    """
    return self._transactions(self.user,
                              recipient_identifier,
                              message,
                              files,
                              files_count)

  # FIXME: Nuke this ! Use the user fetching routines from user.py and
  # don't hardcode the list of fields. Stop ADDING code !
  def __recipient_from_identifier(self,
                                  recipient_identifier,
                                  sender):
    """Get the recipient from identifier. If it doesn't exist, create a ghost.
    recipient_identifier -- The user identifier (can be an email, ObjectId or
                            a phone number)
    sender -- The transaction sender.

    Return ghost, new_user tuple.
    """
    recipient_identifier = utils.identifier(recipient_identifier)
    recipient_fields = self.__user_view_fields + [
      'email',
      'devices.id',
      'features',
      'ghost_code',
      'shorten_ghost_profile_url',
      'emailing.delight',
      'merged_with'
    ]
    # Determine the nature of the recipient identifier.
    recipient_id = None
    recipient = None
    is_a_phone_number = False
    is_an_email = False
    peer_email = None
    phone_number = None
    if isinstance(recipient_identifier, bson.ObjectId):
      recipient = self.__user_fetch(
        {'_id': recipient_identifier},
        fields = recipient_fields)
      if recipient is not None:
        return recipient, False
    is_an_email = utils.is_an_email_address(recipient_identifier)
    if is_an_email:
      elle.log.debug("%s is an email" % recipient_identifier)
      peer_email = recipient_identifier
      # XXX: search email in each accounts.
      recipient = self.user_by_email(peer_email,
                                     fields = recipient_fields,
                                     ensure_existence = False)
    else:
      device = self.current_device
      phone_number = clean_up_phone_number(
        recipient_identifier, device.get('country_code', None))
      is_a_phone_number = phone_number is not None
      if is_a_phone_number:
        elle.log.debug("%s is an phone" % phone_number)
        recipient = self.user_by_phone_number(phone_number,
                                              fields = recipient_fields,
                                              ensure_existence = False)
    assert isinstance(is_a_phone_number, bool)
    assert isinstance(is_an_email, bool)
    if is_a_phone_number is False and is_an_email is False:
      return self.bad_request({
        'reason': 'identifier %s was ill-formed' % recipient_identifier,
        'detail': 'neither a phone number nor an email address ' \
                  '(country code: %s)' % device.get('country_code', None)
      })
    if recipient is None or recipient['register_status'] == 'contact':
      elle.log.trace("recipient unknown, create a ghost")
      new_user = True
      if is_an_email:
        recipient_id = self.__register_ghost({
          'email': peer_email,
          'fullname': peer_email, # This is safe as long as we don't allow searching for ghost users.
          'accounts': [{'type':'email', 'id': peer_email}],
        }, recipient)
      if is_a_phone_number:
        recipient_id = self.__register_ghost({
          'phone_number': phone_number,
          'fullname': phone_number, # Same comment.
          'accounts': [{'type':'phone', 'id': phone_number}],
        }, recipient)
      if recipient_id is None:
        return self.bad_request({
          'reason': 'couldn`t get the ghost recipient_id from ' \
                    'recipient_identifier %s' % recipient_identifier,
          'detail': 'neither a phone number nor an email address ' \
                    '(country code: %s)' % device.get('country_code', None)
      })
      recipient = self.__user_fetch(
        {"_id": recipient_id},
        fields = recipient_fields)
      if self.metrics is not None:
        #FIXME: enable when clients will push their OS in user-agent
        #user_agent = bottle.request.headers['User-Agent']
        user_agent = None
        device = self.current_device
        if device and 'version' in device and 'os' in device:
          user_agent = 'MetaClientProxy/%s.%s.%s (%s)' % (
            device['version']['major'],
            device['version']['minor'],
            device['version']['subminor'],
            device['os']
          )
        self.metrics.send(
          [{
            'event': 'new_ghost',
            'ghost_code': recipient['ghost_code'],
            'user': str(recipient['_id']),
            'features': recipient['features'],
            'sender': str(sender['_id']),
            'timestamp': time.time(),
            'is_email': is_an_email,
          }],
          collection = 'users',
          user_agent = user_agent)
      return recipient, True
    else:
      return recipient, False

  def _transactions(self,
                    sender,
                    recipient_identifier,
                    message,
                    files,
                    files_count):
    recipient_identifier = utils.identifier(recipient_identifier)
    with elle.log.trace("create transaction (recipient %s)" % recipient_identifier):
      recipient, new_user = self.__recipient_from_identifier(
        recipient_identifier,
        sender)
      transaction = {
        'sender_id': bson.ObjectId(sender['_id']),
        'sender_fullname': '',
        'sender_device_id': self.current_device['id'],

        'recipient_id': bson.ObjectId(recipient['_id']),
        'recipient_fullname': '',
        'recipient_device_id': '',
        'involved': ['', recipient['_id']],
        # Empty until accepted.
        'recipient_device_name': '',

        'message': message,

        'files': files,
        'files_count': files_count,
        'total_size': 0,
        'is_directory': False,

        'creation_time': self.now,
        'modification_time': self.now,
        'ctime': time.time(),
        'mtime': time.time(),
        'status': transaction_status.CREATED,
        'fallback_host': None,
        'fallback_port_ssl': None,
        'fallback_port_tcp': None,
        'aws_credentials': None,
        'is_ghost': False,
        'strings': '',

        'cloud_buffered': False,
        'paused': False,
        }
      transaction_id = self.database.transactions.insert(transaction)
      return {
        'created_transaction_id': transaction_id,
      }

  @api('/transaction/create', method = 'POST')
  @require_logged_in
  def transaction_create_api(self,
                             files,
                             files_count,
                             total_size,
                             is_directory,
                             device_id, # Can be determine by session.
                             message = "",
                             recipient_identifier: utils.identifier = None,
                             id_or_email = None):
    return self.transaction_create(
      sender = self.user,
      files = files,
      files_count = files_count,
      total_size = total_size,
      is_directory = is_directory,
      id_or_email = id_or_email,
      recipient_identifier = recipient_identifier,
      device_id = device_id,
      message = message)

  def transaction_create(self,
                         sender,
                         files,
                         files_count,
                         total_size,
                         is_directory,
                         device_id, # Can be determine by session.
                         id_or_email = None,
                         recipient_identifier : utils.identifier = None,
                         message = "",
                         transaction_id = None,
                         recipient_device_id = None):
    """
    Send a file to a specific user.
    If you pass an email and the user is not registered in infinit,
    create a 'ghost' in the database, waiting for him to register.

    files -- the list of files names.
    files_count -- the number of files.
    total_size -- the total size.
    is_directory -- if the sent file is a directory.
    device_id -- the emiter device id.
    id_or_email -- the recipient id or email.
    recipient -- a more generic id_or_email.
    message -- an optional message.
    transaction_id -- id if the transaction was previously created with
    create_empty.

    Errors:
    Using an id that doesn't exist.
    """
    if id_or_email is None and recipient_identifier is None:
      self.bad_request({
        'reason': 'you must provide id_or_email or recipient_identifier'
      })
    recipient_identifier = recipient_identifier or utils.identifier(id_or_email)
    with elle.log.trace("create transaction 2 (recipient %s)" % recipient_identifier):
      recipient, new_user = self.__recipient_from_identifier(recipient_identifier,
                                                             sender)
      if recipient['register_status'] == 'merged':
        merge_target = recipient.get('merged_with', None)
        if merge_target is None:
          self.gone({
              'reason': 'user %s is merged' % recipient['_id'],
              'recipient_id': recipient['_id'],
        })
        recipient, new_user = self.__recipient_from_identifier(merge_target,
                                                               sender)
      if recipient['register_status'] == 'deleted':
        self.gone({
          'reason': 'user %s is deleted' % recipient['_id'],
          'recipient_id': recipient['_id'],
        })
      if recipient_device_id is not None:
        if not any(d['id'] == recipient_device_id for d in recipient['devices']):
          self.not_found({
            'reason': 'no such device for user',
            'user': recipient['_id'],
            'device': recipient_device_id,
          })
      is_ghost = recipient['register_status'] == 'ghost'
      # FIXME: restore when clients will handle it.
      # if is_ghost and total_size > 2000000000:
      #   self.forbidden({
      #     'reason': 'transaction to non-existing users limited to 2GB',
      #    })

      if is_ghost:
        # Add to referral
        self.database.users.update(
          {'_id': recipient['_id']},
          {'$addToSet': {'referred_by': sender['_id']}}
        )
      elle.log.debug("transaction recipient has id %s" % recipient['_id'])
      _id = sender['_id']
      elle.log.debug('Sender agent %s, version %s, peer_new %s peer_ghost %s'
                     % (self.user_agent, self.user_version, new_user,  is_ghost))
      transaction = {
        'sender_id': _id,
        'sender_fullname': sender['fullname'],
        'sender_device_id': device_id, # bson.ObjectId(device_id),
        'recipient_id': recipient['_id'], #X
        'recipient_fullname': recipient['fullname'],
        'recipient_device_id': recipient_device_id if recipient_device_id else '',
        'involved': [_id, recipient['_id']],
        # Empty until accepted.
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
        'paused': False,
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
      if transaction_id is not None:
        transaction['status'] = transaction_status.INITIALIZED
        self.database.transactions.update(
            {'_id': transaction_id},
            {'$set': transaction})
        transaction['_id'] = transaction_id
        self.notifier.notify_some(
          notifier.PEER_TRANSACTION,
          recipient_ids = {transaction['recipient_id']},
          message = transaction,
        )
        self.__update_transaction_stats(
          sender,
          counts = ['sent_peer', 'sent'],
          pending = transaction,
          time = True)
        if not is_ghost:
          self.__update_transaction_stats(
            recipient,
            unaccepted = transaction,
            time = False)
      else:
        transaction_id = self.database.transactions.insert(transaction)
      transaction['_id'] = transaction_id
      self._increase_swag(sender['_id'], recipient['_id'])
      if  recipient_device_id is None and recipient['_id'] == sender['_id']:
        self.__delight(recipient, 'Send to Self',
                       transaction, recipient)
      recipient_view = self.__user_view(recipient)
      return self.success({
          'created_transaction_id': transaction_id,
          'remaining_invitations': sender.get('remaining_invitations', 0),
          'recipient_is_ghost': is_ghost,
          'recipient': recipient_view,
        })

  def __delight(self, user, template, transaction, peer):
    if 'delight' not in user.get('emailing', {}):
      self.database.users.update(
        {'_id': user['_id']},
        {'$set': {'emailing.delight': template}},
      )
      self.emailer.send_one(
        template,
        recipient_email = user['email'],
        recipient_name = user['fullname'],
        variables = {
          'user': self.email_user_vars(user),
          'peer': self.email_user_vars(peer),
          'transaction':
            self.email_transaction_vars(transaction, user),
          'login_token': self.login_token(user['email']),
        },
      )

  def __update_transaction_stats(self,
                                 user,
                                 time = True,
                                 counts = None,
                                 pending = None,
                                 unaccepted = None):
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
    for t, f in [(pending, 'pending'), (unaccepted, 'unaccepted')]:
      if t is not None:
        update.setdefault('$set', {})
        update.setdefault('$push', {})
        update['$push']['transactions.%s' % f] = t['_id']
        update['$set']['transactions.%s_has' % f] = True
        update['$set']['transactions.activity_has'] = True
    self.database.users.update({'_id': user}, update)

  def __complete_transaction_pending_stats(self, user, transaction):
    self.__complete_transaction_stats(user, transaction, 'pending')


  def __complete_transaction_unaccepted_stats(self, user, transaction):
    self.__complete_transaction_stats(user, transaction, 'unaccepted')

  def __complete_transaction_stats(self, user, transaction, f):
    if isinstance(user, dict):
      user = user['_id']
    res = self.database.users.update(
      {'_id': user},
      {'$pull': {'transactions.%s' % f: transaction['_id']}},
    )
    if res['n']:
      self.database.users.update(
        {'_id': user, 'transactions.%s' % f: []},
        {'$set': {'transactions.%s_has' % f: False}},
      )
      self.database.users.update(
        {
          '_id': user,
          'transactions.pending_has': {'$ne': True},
          'transactions.unaccepted_has':  {'$ne': True},
        },
        {
          '$set':
          {
            'transactions.activity_has': False,
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
      self.__complete_transaction_unaccepted_stats(user, transaction)
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
          'aws_credentials': credentials
        })
      return res

  def on_reject(self, transaction, user, device_id, device_name):
    with elle.log.trace("reject transaction as %s" % device_id):
      if device_id is None or device_name is None:
        return {} # Backwards compatibility < 0.9.30
      device_id = uuid.UUID(device_id)
      if str(device_id) not in map(lambda x: x['id'], user['devices']):
        raise error.Error(error.DEVICE_DOESNT_BELONG_TO_YOU)
      res = {
        'recipient_fullname': user['fullname'],
        'recipient_device_name' : device_name,
        'recipient_device_id': str(device_id)
      }
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

  def __view_transaction_email(self, transaction):
    return {

    }

  def on_ghost_uploaded(self, transaction, device_id, device_name, user):
    elle.log.log('Transaction finished');
    # Guess if this was a ghost cloud upload or not
    recipient = self.__user_fetch(
      transaction['recipient_id'],
      fields = self.__user_view_fields + \
        ['email', 'ghost_code', 'shorten_ghost_profile_url'])
    if recipient['register_status'] == 'deleted':
      self.gone({
        'reason': 'user %s is deleted' % recipient['_id'],
        'recipient_id': recipient['_id'],
      })
    elle.log.log('Peer status: %s' % recipient['register_status'])
    elle.log.log('transaction: %s' % transaction.keys())
    peer_email = recipient.get('email', '')
    if transaction.get('is_ghost', False):
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
        bucket = transaction['aws_credentials']['bucket']
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
      if peer_email:
        variables = {
          'sender': self.email_user_vars(user),
          'ghost_email': peer_email,
          'transaction':
            self.email_transaction_vars(transaction, recipient),
          'ghost_profile': recipient.get(
            'shorten_ghost_profile_url',
              self.__ghost_profile_url(recipient, type = "email")),
        }
        # Ghost created pre 0.9.30 has no ghost code.
        if 'ghost_code' in recipient:
          variables.update({
            'ghost_code': recipient['ghost_code'],
          })
          variables['sender']['avatar'] += '?ghost_code=%s' % recipient['ghost_code']
        self.emailer.send_one(
          'Transfer (Initial)',
          recipient_email = peer_email,
          sender_name = '%s via Infinit' % user['fullname'],
          variables = variables,
        )
      return {
        'transaction_hash': transaction_hash,
        'download_link': ghost_get_url,
      }
    else:
      elle.log.trace('Recipient is not a ghost, nothing to do')
      return {}

  @api('/transaction/update', method = 'POST')
  @require_logged_in_fields(['emailing.delight'])
  def transaction_update(self,
                         transaction_id,
                         status = None,
                         device_id = None,
                         device_name = None,
                         paused = None):

    try:
      res = self._transaction_update(transaction_id,
                                     status,
                                     device_id,
                                     device_name,
                                     self.user,
                                     paused)
    except error.Error as e:
      return self.fail(*e.args)

    return self.success(res)

  def _transaction_update(self,
                          transaction_id,
                          status = None,
                          device_id = None,
                          device_name = None,
                          user = None,
                          paused = None):
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
      if is_sender:
        sender = user
        recipient = self.__user_fetch(
          {'_id': transaction['recipient_id']},
          fields = self.__user_self_fields + ['emailing.delight'],
        )
      else:
        recipient = user
        sender = self.__user_fetch(
          {'_id': transaction['sender_id']},
          fields = self.__user_self_fields + ['emailing.delight'],
        )
      diff = {}
      operation = {}
      if status is not None:
        args = (transaction['status'], status)
        if transaction['status'] == status:
          return {}
        allowed = transaction_status.transitions[transaction['status']][is_sender]
        if status not in allowed:
          msg = 'changing status %s to %s not permitted' % args
          elle.log.trace(msg)
          response(403, {'reason': msg, 'sender': is_sender})
        if transaction['status'] in transaction_status.final:
          msg = 'changing final status %s to %s not permitted' % args
          elle.log.trace(msg)
          response(403, {'reason': msg})
        if status == transaction_status.ACCEPTED:
          diff.update(self.on_accept(transaction = transaction,
                                     user = user,
                                     device_id = device_id,
                                     device_name = device_name))
        elif status == transaction_status.REJECTED:
          self.__complete_transaction_unaccepted_stats(
            transaction['recipient_id'], transaction)
          diff.update(self.on_reject(transaction = transaction,
                                     user = user,
                                     device_id = device_id,
                                     device_name = device_name))
          diff.update(self.cloud_cleanup_transaction(transaction = transaction))
        elif status == transaction_status.GHOST_UPLOADED:
          self.__delight(user, 'Shared', transaction, recipient)
          diff.update(self.on_ghost_uploaded(transaction = transaction,
                                       device_id = device_id,
                                       device_name = device_name,
                                       user = user))
        elif status == transaction_status.CANCELED:
          self.__complete_transaction_unaccepted_stats(
            transaction['recipient_id'], transaction)
          if not transaction.get('canceler', None):
            diff.update({'canceler': {'user': user['_id'], 'device': device_id}})
            diff.update(self.cloud_cleanup_transaction(transaction = transaction))
        elif status == transaction_status.FAILED:
          self.__complete_transaction_unaccepted_stats(
            transaction['recipient_id'], transaction)
          diff.update(self.cloud_cleanup_transaction(transaction = transaction))
        elif status == transaction_status.FINISHED:
          self.__delight(sender, 'Shared', transaction, recipient)
          self.__update_transaction_stats(
            transaction['sender_id'],
            counts = ['reached_peer', 'reached'],
            time = False)
          self.__delight(recipient, 'Received', transaction, sender)
          self.__update_transaction_stats(
            transaction['recipient_id'],
            counts = ['received_peer', 'received'],
            time = True)
        if status in transaction_status.final:
          operation["$unset"] = {"nodes": 1}
        # Don't override accepted with cloud_buffered.
        if status == transaction_status.CLOUD_BUFFERED and \
           transaction['status'] == transaction_status.ACCEPTED:
          diff.update({'status': transaction_status.ACCEPTED})
        elif status == transaction_status.CLOUD_BUFFERED:
          self.__delight(sender, 'Shared', transaction, recipient)
          diff.update({
            'status': status,
            'cloud_buffered': True
          })
        else:
          diff.update({'status': status})
      if paused is not None:
        diff.update({
          'paused': paused,
        })
        self.notifier.notify_some(
          notifier.PAUSED,
          recipient_ids = {transaction['sender_id'],
                           transaction['recipient_id']},
          message = {
            'transaction_id': transaction_id,
            'paused': paused,
            })
      # Don't update with an empty dictionary: it would empty the
      # object.
      if diff:
        diff.update({
          'mtime': time.time(),
          'modification_time': self.now,
        })
        operation["$set"] = diff
        if status in transaction_status.final:
          for i in ['recipient_id', 'sender_id']:
            self.__complete_transaction_pending_stats(
              transaction[i], transaction)
        elif status in [transaction_status.statuses['ghost_uploaded'],
                        transaction_status.statuses['cloud_buffered']]:
          self.__complete_transaction_pending_stats(
            transaction['sender_id'], transaction)
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
    return transaction['nodes']

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
    existing_creds = transaction.get('aws_credentials', None)
    if not force_regenerate:
      if existing_creds is not None:
        elle.log.debug('cloud_buffer: returning from cache')
        existing_creds['current_time'] = current_time
        return self.success(existing_creds)
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
      if existing_creds is not None and existing_creds.get('bucket', None) is not None:
        bucket = existing_creds['bucket']
      elif transaction['is_ghost']:
        bucket = self.aws_invite_bucket
      else:
        bucket = self.aws_buffer_bucket
      token_maker = cloud_buffer_token.CloudBufferToken(
        user['_id'], transaction_id, 'ALL',
        aws_region = self.aws_region, bucket_name = bucket)
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
      credentials['bucket']            = bucket
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
