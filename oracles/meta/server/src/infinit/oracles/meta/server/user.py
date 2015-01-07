# -*- encoding: utf-8 -*-

import bottle
import bson
import datetime
import json
import random
import uuid

import elle.log
import papier

from .plugins.response import response, Response
from .utils import api, require_logged_in, require_admin, require_logged_in_or_admin, hash_pasword, json_value, require_key, key
from . import error, notifier, regexp, conf, invitation, mail

from pymongo import DESCENDING
import os
import string
import time
import unicodedata

#
# Users
#
ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.User'

class Mixin:

  ## ------ ##
  ## Handle ##
  ## ------ ##

  def __generate_handle(self,
                        fullname,
                        enlarge = True):
    assert isinstance(fullname, str)
    with elle.log.trace("generate handle from fullname %s" % fullname):
      allowed_characters = string.ascii_letters + string.digits
      allowed_characters += '_'
      normalized_name = unicodedata.normalize('NFKD', fullname.strip().replace(' ', '_'))
      handle = ''
      for c in normalized_name:
        if c in allowed_characters:
          handle += c

      elle.log.debug("clean handle: %s" % handle)

      if len(handle) > 30:
        handle = handle[:30]

      if not enlarge:
        return handle

      return handle

  def generate_handle(self,
                      fullname):
    """ Generate handle from a given fullname.

    fullname -- plain text user fullname.
    """
    return self.__generate_handle(fullname)

  def unique_handle(self,
                    fullname):
    h = self.__generate_handle(fullname)
    while self.user_by_handle(h, ensure_existence = False):
      h += str(int(random.random() * 10))
    return h



  ## -------- ##
  ## Sessions ##
  ## -------- ##

  def _forbidden_with_error(self, error):
    ret_msg = {'code': error[0], 'message': error[1]}
    response(403, ret_msg)

  def _login(self, email, password):
    try:
      user = self.user_by_email_password(
        email, password, ensure_existence = True)
      # If email confirmed is not present, we can consider it's an old user,
      # so his address will not be confirmed.
      if not user.get('email_confirmed', True):
        from time import time
        if time() > user['unconfirmed_email_deadline']:
          self.resend_confirmation_email(email)
          self._forbidden_with_error(error.EMAIL_NOT_CONFIRMED)
      return user
    except error.Error as e:
      args = e.args
      self._forbidden_with_error(args[0])

  def _login_response(self,
                      user,
                      device = None,
                      web = False):
    if self.user_version >= (0, 9, 25) and not web:
      assert device is not None
      return {'self': self._user_self()}
    else:
      res = {
        '_id' : user['_id'],
        'fullname': user['fullname'],
        'email': user['email'],
        'handle': user['handle'],
        'register_status': user['register_status'],
      }
      if not web:
        assert device is not None
        res.update({
          'identity': user['identity'],
          'device_id': device['id'],
        })
      if not user.get('email_confirmed', True):
        from time import time
        res.update({
          'unconfirmed_email_leeway': user['unconfirmed_email_deadline'] - time()
        })
      return res

  @api('/login', method = 'POST')
  def login(self,
            email,
            password,
            device_id: uuid.UUID,
            OS: str = None,
            pick_trophonius: bool = True):
    email = email.replace(' ', '')
    if OS is not None:
      OS = OS.strip().lower()
    # FIXME: 0.0.0.0 is the website.
    if self.user_version < (0, 9, 0) and self.user_version != (0, 0, 0):
      return self.fail(error.DEPRECATED)
    with elle.log.trace("%s: log on device %s" % (email, device_id)):
      assert isinstance(device_id, uuid.UUID)
      email = email.lower()
      user = self._login(email, password)
      # If creation process was interrupted, generate identity now.
      if 'public_key' not in user:
        self.__generate_identity(user['_id'], email, password)
      query = {'id': str(device_id), 'owner': user['_id']}
      elle.log.debug("%s: look for session" % email)
      device = self.device(ensure_existence = False, **query)
      if device is None:
        elle.log.trace("user logged with an unknown device")
        device = self._create_device(id = device_id,
                                     owner = user)
      else:
        assert str(device_id) in user['devices']
      # Remove potential leaked previous session.
      self.sessions.remove({'email': email, 'device': device['_id']})
      elle.log.debug("%s: store session" % email)
      bottle.request.session['device'] = device['_id']
      bottle.request.session['email'] = email

      user = self.user
      self.database.users.find_and_modify(
        query = {'_id': user['_id']},
        update = {'$set': {'last_connection': time.time(),}})
      elle.log.trace("%s: successfully connected as %s on device %s" %
                     (email, user['_id'], device['id']))
      if OS is not None and OS in invitation.os_lists.keys():
        elle.log.debug("connected on os: %s" % OS)
        res = self.database.users.find_and_modify({"_id": user['_id'], "os.%s" % OS: None},
                                                  {"$addToSet": {"os": OS}})
        # Because new is not set, find_and_modify will return the non modified user:
        # - os was not present.
        # - os was present but the os was not in the list.
        if res is not None and ('os' not in res or OS not in res['os']):
          self.invitation.subscribe(list_name = OS,
                                    email = user['email'])
      else:
        elle.log.debug("%s: no OS specified" % user['email'])
      response = self.success(self._login_response(user,
                                                   device = device,
                                                   web = False))
      if pick_trophonius:
        response['trophonius'] = self.trophonius_pick()
      # Update missing features
      current_features = user.get('features', {})
      features = self._roll_features(False, current_features)
      if features != current_features:
        self.database.users.update(
          {'_id': user['_id']},
          {'$set': { 'features': features}})
      response['features'] = list(features.items())
      response['device'] = device
      return response

  @api('/web-login', method = 'POST')
  def web_login(self,
                email,
                password):
    email = email.replace(' ', '')
    with elle.log.trace("%s: web login" % email):
      user = self._login(email, password)
      elle.log.debug("%s: store session" % email)
      bottle.request.session['email'] = email
      user = self.user
      elle.log.trace("%s: successfully connected as %s" %
                     (email, user['_id']))
      return self.success(self._login_response(user, web = True))

  @api('/logout', method = 'POST')
  @require_logged_in
  def logout(self):
    user = self.user
    with elle.log.trace("%s: logout" % user['email']):
      if 'email' in bottle.request.session:
        elle.log.debug("%s: remove session" % user['email'])
        # Web sessions have no device.
        if 'device' in bottle.request.session:
          elle.log.debug("%s: remove session device" % user['email'])
          del bottle.request.session['device']
        del bottle.request.session['email']
        return self.success()
      else:
        return self.fail(error.NOT_LOGGED_IN)

  def kickout(self, reason, user = None):
    if user is None:
      user = self.user
    with elle.log.trace('kickout %s: %s' % (user['_id'], reason)):
      self.database.sessions.remove({'email': user['email']})
      self.notifier.notify_some(
        notifier.INVALID_CREDENTIALS,
        recipient_ids = {user['_id']},
        message = {'response_details': reason})

  @property
  def user(self):
    elle.log.trace("get user from session")
    if not hasattr(bottle.request, 'session'):
      return None
    email = bottle.request.session.get('email', None)
    if email is not None:
      return self.user_by_email(email, ensure_existence = False)
    elle.log.trace("session not found")

  ## -------- ##
  ## Register ##
  ## -------- ##

  def _register(self, **kwargs):
    kwargs['connected'] = False
    user = self.database.users.save(kwargs)
    return user

  @api('/user/register', method = 'POST')
  def user_register_api(self,
                        email,
                        password,
                        fullname,
                        source = None,
                        activation_code = None):
    if self.user is not None:
      return self.fail(error.ALREADY_LOGGED_IN)
    try:
      user = self.user_register(email = email,
                                password = password,
                                fullname = fullname,
                                source = source,
                                activation_code = activation_code)
      return self.success({
        'registered_user_id': user['_id'],
        'invitation_source': '',
        'unconfirmed_email_leeway': self.unconfirmed_email_leeway,
      })
    except Exception as e:
      return self.fail(e.args[0])


  def user_register(self,
                    email,
                    password,
                    fullname,
                    source = None,
                    activation_code = None):
    """Register a new user.

    email -- the account email.
    password -- the client side hashed password.
    fullname -- the user fullname.
    activation_code -- the activation code.
    """
    email = email.replace(' ', '')
    _validators = [
      (email, regexp.EmailValidator),
      (password, regexp.PasswordValidator),
      (fullname, regexp.FullnameValidator),
    ]
    for arg, validator in _validators:
      res = validator(arg)
      if res != 0:
        raise Exception(res)
    fullname = fullname.strip()
    with elle.log.trace("registration: %s as %s" % (email, fullname)):
      email = email.strip().lower()
      import hashlib
      hash = str(time.time()) + email
      hash = hash.encode('utf-8')
      hash = hashlib.md5(hash).hexdigest()
      handle = self.unique_handle(fullname)
      user_content = {
        'connected': False,
        'features': self._roll_features(True),
        'register_status': 'ok',
        'email': email,
        'fullname': fullname,
        'password': hash_pasword(password),
        'handle': handle,
        'lw_handle': handle.lower(),
        'swaggers': {},
        'networks': [],
        'devices': [],
        'connected_devices': [],
        'notifications': [],
        'old_notifications': [],
        'accounts': [
          {'type':'email', 'id': email}
        ],
        'creation_time': self.now,
        'email_confirmed': False,
        'unconfirmed_email_deadline':
          time.time() + self.unconfirmed_email_leeway,
        'email_confirmation_hash': str(hash),
      }
      if source is not None:
        user_content['source'] = source
      res = self.database.users.find_and_modify(
        query = {
          'accounts.id': email,
        },
        update = {
          '$setOnInsert': user_content,
        },
        full_response = True,
        new = True,
        upsert = True,
      )
      user = res['value']
      if res['lastErrorObject']['updatedExisting']:
        if user['register_status'] == 'ghost':
          for field in ['swaggers', 'features']:
            user_content[field] = user[field]
          user = self.database.users.find_and_modify(
            query = {
              'accounts.id': email,
              'register_status': 'ghost',
            },
            update = {
              '$set': user_content,
            },
            new = True,
            upsert = False,
          )
          if user is None:
            # The ghost was already transformed - prevent the race
            # condition.
            raise Exception(error.EMAIL_ALREADY_REGISTERED)
        else:
          # The user existed.
          raise Exception(error.EMAIL_ALREADY_REGISTERED)
      user_id = user['_id']
      self.__generate_identity(user_id, email, password)
      with elle.log.trace("add user to the mailing list"):
        self.invitation.subscribe(email)
      self._notify_swaggers(
        notifier.NEW_SWAGGER,
        {
          'user_id' : str(user_id),
        },
        user_id = user_id,
      )
      user = self.user_by_email(email, ensure_existence = True)
      self.mailer.send_template(
        to = user['email'],
        template_name = 'confirm-sign-up',
        merge_vars = {
          user['email']: {
            'CONFIRM_KEY': key('/users/%s/confirm-email' % user['_id']),
            'USER_FULLNAME': user['fullname'],
            'USER_ID': str(user['_id']),
          }}
      )
      return user

  def __generate_identity(self, user_id, email, password):
    with elle.log.trace('generate identity'):
      identity, public_key = papier.generate_identity(
        str(user_id),
        email,
        password,
        conf.INFINIT_AUTHORITY_PATH,
        conf.INFINIT_AUTHORITY_PASSWORD
        )
      self.database.users.find_and_modify(
        {
          '_id': user_id,
        },
        {
          '$set':
          {
            'identity': identity,
            'public_key': public_key,
          },
        },
      )

  def __account_from_hash(self, hash):
    with elle.log.debug('get user account from hash %s' % hash):
      user = self.database.users.find_one({"email_confirmation_hash": hash})
      if user is None:
        raise error.Error(
          error.OPERATION_NOT_PERMITTED,
          "No user could be found",
        )
      return user

  # Deprecated
  @api('/user/confirm_email/<hash>', method = 'POST')
  def _confirm_email(self,
                    hash: str):
    with elle.log.trace('confirm email'):
      try:
        user = self.__account_from_hash(hash)
        elle.log.trace('confirm %s\'s account' % user['email'])
        self.database.users.find_and_modify(
          query = {"email_confirmation_hash": hash},
          update = {
            '$unset': {'unconfirmed_email_leeway': True},
            '$set': {'email_confirmed': True}
            })
        return self.success()
      except error.Error as e:
        self.fail(*e.args)

  @api('/users/<user>/confirm-email', method = 'POST')
  def confirm_email(self,
                    user,
                    hash: str = None,
                    key: str = None):
    with elle.log.trace('confirm email for %s' % user):
      if '@' in user:
        query = {'email': user}
      else:
        query = {'_id': bson.ObjectId(user)}
      if not self.admin:
        if key is not None:
          self.check_key(key)
        elif hash is not None:
          query['email_confirmation_hash'] = hash
      res = self.database.users.find_and_modify(
        query = query,
        update = {
          '$unset': {'unconfirmed_email_leeway': True},
          '$set': {'email_confirmed': True}
        })
      if res is None:
        self.forbidden({
          'user': user,
          'reason': 'invalid confirmation hash or email',
        })
      return {}

  # Deprecated
  @api('/user/resend_confirmation_email/<email>', method = 'POST')
  def _resend_confirmation_email(self,
                                email: str):
    try:
      self.resend_confirmation_email(email)
    except Response:
      self.fail(error.EMAIL_ALREADY_CONFIRMED)
    return self.success()

  @api('/users/<user>/resend-confirmation-email', method = 'POST')
  def resend_confirmation_email(self,
                                user: str):
    with elle.log.trace(
        'resending confirmation email request for %s' % user):
      user = self.user_by_id_or_email(user)
      if user is None:
        response(404, {'reason': 'user not found'})
      if user.get('email_confirmed', True):
        response(404, {'reason': 'email is already confirmed'})
      assert user.get('email_confirmation_hash') is not None
      # XXX: Waiting for mandrill to put cooldown on mail.
      now = self.now
      confirmation_cooldown = now - self.email_confirmation_cooldown
      res = self.database.users.find_and_modify(
        {
          'email': user['email'],
          '$or': [
            {
              'last_email_confirmation':
              {
                '$lt': confirmation_cooldown,
              }
            },
            {
              'last_email_confirmation':
              {
                '$exists': False,
              }
            }
          ]
        },
        {
          '$set':
          {
            'last_email_confirmation': now,
          }
        })
      if res is not None:
        self.mailer.send_template(
          to = user['email'],
          template_name = 'reconfirm-sign-up',
          merge_vars = {
            user['email']: {
              'CONFIRM_KEY': key('/users/%s/confirm-email' % user['_id']),
              'USER_FULLNAME': user['fullname'],
              'USER_ID': str(user['_id']),
              }}
          )
      return {}

  @api('/user/<id>/connected')
  def is_connected(self, id: bson.ObjectId):
    try:
      return self.success({"connected": self._is_connected(id)})
    except error.Error as e:
      self.fail(*e.args)

  @api('/user/change_email_request', method = 'POST')
  @require_logged_in
  def change_email_request(self,
                           new_email,
                           password):
    """
    Request to change the main email address for the account.
    The main email is used for login and email alerts.

    new_email -- The new main email address.
    password -- The password of the account.
    """
    user = self.user
    with elle.log.trace('request to change main email from %s to %s' %
                        (user['email'], new_email)):
      _validators = [
        (new_email, regexp.EmailValidator),
        (password, regexp.PasswordValidator),
      ]

      for arg, validator in _validators:
        res = validator(arg)
        if res != 0:
          return self._forbidden_with_error(error.EMAIL_NOT_VALID)

      new_email = new_email.lower().strip()
      # Check if the new address is already in use.
      if self.user_by_email(new_email, ensure_existence = False) is not None:
        return self._forbidden_with_error(error.EMAIL_ALREADY_REGISTERED)
      if hash_pasword(password) != user['password']:
        return self._forbidden_with_error(error.PASSWORD_NOT_VALID)
      from time import time
      import hashlib
      seed = str(time()) + new_email
      hash = hashlib.md5(seed.encode('utf-8')).hexdigest()
      res = self.mailer.send_template(
        new_email,
        'change-email-address',
        merge_vars = {
          new_email : {
            "hash": hash,
            "new_email_address": new_email,
            "user_fullname": user['fullname']
          }
        })
      self.database.users.find_and_modify(
        { "_id": user['_id'] },
        {
          "$set": {
            "new_main_email": new_email,
            "new_main_email_hash": hash,
          }
        })
      return {}

  @api('/user/change_email/<hash>', method = 'GET')
  def new_email_from_hash(self, hash):
    """
    When changing a user's email address, this returns the new email address from
    a given hash.
    hash -- Hash stored in DB for changing email address (new_main_email_hash).
    """
    with elle.log.trace('fetch new email address from hash: %s' % hash):
      user = self.database.users.find_one({'new_main_email_hash': hash})
      if user is None:
        return self.not_found()
      return {'new_email': user['new_main_email']}

  @api('/user/change_email', method = 'POST')
  def change_email(self,
                   hash,
                   password):
    """
    Validate and perform change of the user's main email address.
    Included in this process is the creation of a new identity and rehashing of
    the user's password. This means that the user is logged out of current
    sessions and that their transactions are cancelled.

    hash -- The hash stored on the DB for the operation (new_main_email_hash).
    password -- User's password hashed with new email address.
    """
    with elle.log.trace('change mail email associated to %s' % hash):
      _validators = [
        (password, regexp.PasswordValidator),
      ]

      for arg, validator in _validators:
        res = validator(arg)
        if res != 0:
          return self._forbidden_with_error(error.PASSWORD_NOT_VALID)
      # Check that the hash exists and pull user based on it.
      user = self.database.users.find_one({'new_main_email_hash': hash})
      if user is None:
        return self._forbidden_with_error(error.UNKNOWN_USER)
      # Check that the email has not been registered.
      new_email = user['new_main_email']
      if self.user_by_email(new_email, ensure_existence = False) is not None:
        return self._forbidden_with_error(error.EMAIL_ALREADY_REGISTERED)
      # Invalidate credentials.
      self.sessions.remove({'email': user['email'], 'device': ''})
      # Kick them out of the app.
      self.notifier.notify_some(
        notifier.INVALID_CREDENTIALS,
        recipient_ids = {user['_id']},
        message = {'response_details': 'user email changed'})
      # Cancel transactions as identity will change.
      self.cancel_transactions(user)
      # Handle mailing list subscriptions.
      self.invitation.unsubscribe(user['email'])
      self.invitation.subscribe(new_email)
      with elle.log.trace('generate identity'):
        identity, public_key = papier.generate_identity(
          str(user['_id']),  # Unique ID.
          new_email,         # Description.
          password,          # Password.
          conf.INFINIT_AUTHORITY_PATH,
          conf.INFINIT_AUTHORITY_PASSWORD
        )
      # Update user in DB.
      # We keep the user's old email address in the accounts section.
      self.database.users.update(
        {'_id': user['_id']},
        {
          '$set':
          {
            'email': new_email,
            'email_confirmed': True,
            'password': hash_pasword(password),
            'identity': identity,
            'public_key': public_key,
          },
          '$unset':
          {
            'new_main_email': '',
            'new_main_email_hash': '',
          },
          '$addToSet':
          {
            'accounts': {'type': 'email', 'id': new_email}
          },
        })
      return {}

  @api('/user/change_password', method = 'POST')
  @require_logged_in
  def change_password(self, old_password, new_password):
    """
    Change the user's password.
    old_password -- the user's old password
    new_password -- the user's new password
    """
    # Check that the user's passwords are of the correct form.
    _validators = [
      (old_password, regexp.PasswordValidator),
      (new_password, regexp.PasswordValidator),
    ]

    for arg, validator in _validators:
      res = validator(arg)
      if res != 0:
        return self.fail(res)

    user = self.user
    if user['password'] != hash_pasword(old_password):
      return self.fail(error.PASSWORD_NOT_VALID)
    # Invalidate credentials.
    self.sessions.remove({'email': self.user['email'], 'device': ''})
    # Kick them out of the app.
    self.notifier.notify_some(
      notifier.INVALID_CREDENTIALS,
      recipient_ids = {self.user['_id']},
      message = {'response_details': 'user password changed'})
    self.logout()
    # Cancel transactions as identity will change.
    self.cancel_transactions(user)

    with elle.log.trace('generate identity'):
      identity, public_key = papier.generate_identity(
        str(user['_id']),  # Unique ID.
        user['email'],    # Description.
        new_password,     # Password.
        conf.INFINIT_AUTHORITY_PATH,
        conf.INFINIT_AUTHORITY_PASSWORD
      )

    self.database.users.find_and_modify(
     {'_id': user['_id']},
     {'$set': {
      'password': hash_pasword(new_password),
      'identity': identity,
      'public_key': public_key}})
    return self.success()

  ## ------ ##
  ## Delete ##
  ## ------ ##

  @api('/user', method = 'DELETE')
  @require_logged_in
  def user_delete_self(self):
    user = self.user
    self.logout()
    self.user_delete(user)

  @api('/users/<user>', method = 'DELETE')
  @require_admin
  def user_delete_specifyc(self, user: str):
    self.user_delete(self.user_by_id_or_email(user))

  def user_delete(self, user):
    """The idea is to just keep the user's id and fullname so that transactions
       can still be shown properly to other users. We will leave them in other
       users's swagger lists as we may want to generate the transaction history
       from swaggers rather than transactions.

       We need to:
       - Log the user out and invalidate his session.
       - Delete as much of his information as possible.
       - Remove user as a favourite for other users.
       - Remove user from mailing lists.

       Considerations:
       - Keep the process as atomic at the DB level as possible.
    """
    # Invalidate credentials.
    self.sessions.remove({'email': user['email'], 'device': ''})
    # Kick them out of the app.
    self.notifier.notify_some(
      notifier.INVALID_CREDENTIALS,
      recipient_ids = {user['_id']},
      message = {'response_details': 'user deleted'})
    self.invitation.unsubscribe(user['email'])
    self.cancel_transactions(user)
    self.delete_all_links(user)
    self.remove_devices(user)
    try:
      user.pop('avatar')
    except:
      elle.log.debug('user has no avatar')
    swaggers = set(map(bson.ObjectId, user['swaggers'].keys()))
    self.database.users.update(
      {'_id': user['_id']},
      {
        '$set':
        {
          'accounts': [],
          'connected': False,
          'connected_devices': [],
          'devices': [],
          'email': '',
          'favorites': [],
          'handle': '',
          'identity': '',
          'lw_handle': '',
          'networks': [],
          'notifications': [],
          'old_notifications': [],
          'password': '',
          'public_key': '',
          'register_status': 'deleted',
          'swaggers': {},
        },
        '$unset':
        {
          'avatar': ''
        }
      })
    self.notifier.notify_some(notifier.DELETED_SWAGGER,
      recipient_ids = swaggers,
      message = {'user_id': user['_id']})
    self.remove_user_as_favorite_and_notify(user)
    return self.success()

  def remove_user_as_favorite_and_notify(self, user):
    user_id = user['_id']
    recipient_ids = [str(u['_id']) for u in self.database.users.find(
      {'favorites': user_id},
      fields = ['_id']
    )]
    recipient_ids = set(map(bson.ObjectId, recipient_ids))
    if len(recipient_ids) == 0:
      return
    self.notifier.notify_some(notifier.DELETED_FAVORITE,
      recipient_ids = recipient_ids,
      message = {'user_id': user_id})
    recipient_ids = self.database.users.update(
      {'favorites': user_id},
      {'$pull': {'favorites': user_id}},
      multi = True,
    )

  def remove_swaggers_and_notify(self, user):
    user_id = user['_id']
    swaggers = self.database.users.find_one(
      {'_id': user_id},
      fields = ['swaggers']
    )
    swaggers = list(map(bson.ObjectId, swaggers['swaggers'].keys()))
    if len(swaggers) == 0:
      return
    self.database.users.update(
      {'_id': {'$in': swaggers}},
      {'$unset': {'swaggers.%s' % user_id: ''}},
      multi = True,
    )
    self._notify_swaggers(notifier.DELETED_SWAGGER,
                          {'user_id': bson.ObjectId(user_id)},
                          user_id)
    user = self.database.users.find_and_modify(
      {'_id': user_id},
      {'$set': {'swaggers': {}}},
      new = True
    )

  ## -------------- ##
  ## Search helpers ##
  ## -------------- ##
  def __ensure_user_existence(self, user):
    """Raise if the given user is not valid.

    user -- the user to validate.
    """
    if user is None:
      raise error.Error(error.UNKNOWN_USER)

  def _user_by_id(self, _id, ensure_existence = True):
    """Get a user using by id.

    _id -- the _id of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    assert isinstance(_id, bson.ObjectId)
    user = self.database.users.find_one(_id)
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_public_key(self, key, ensure_existence = True):
    """Get a user from is public_key.

    public_key -- the public_key of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    user = self.database.users.find_one({'public_key': key})
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_email(self, email, ensure_existence = True):
    """Get a user with given email.

    email -- the email of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    email = email.lower().strip()
    user = self.database.users.find_one({'email': email})
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_email_password(self, email, password, ensure_existence = True):
    """Get a user from is email.

    email -- The email of the user.
    password -- The password for that account.
    ensure_existence -- if set, raise if user is invald.
    """
    user = self.database.users.find_one({
      'email': email,
      'password': hash_pasword(password),
    })
    if user is None and ensure_existence:
      raise error.Error(error.EMAIL_PASSWORD_DONT_MATCH)
    return user

  def user_by_handle(self, handle, ensure_existence = True):
    """Get a user from is handle.

    handle -- the handle of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    user = self.database.users.find_one({'lw_handle': handle.lower()})
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  ## ------ ##
  ## Search ##
  ## ------ ##

  @property
  def user_public_fields(self):
    res = {
      'id': '$_id',
      'public_key': '$public_key',
      'fullname': '$fullname',
      'handle': '$handle',
      'connected_devices': '$connected_devices',
      'status': '$connected',
      'register_status': '$register_status',
      '_id': False,
    }
    if self.admin:
      res['features'] = '$features'
      res['creation_time'] = '$creation_time'
      res['email'] = '$email'
      res['email_confirmed'] = '$email_confirmed'
      res['os'] = '$os'
    return res

  def __object_id(self, id):
    try:
      return bson.ObjectId(id.strip())
    except bson.errors.InvalidId:
      self.bad_request({
        'reason': 'invalid id',
        'id': id,
      })

  @api('/users')
  @require_logged_in_or_admin
  def users(self, search = None, limit : int = 5, skip : int = 0, ids = None):
    """Search the ids of the users with handle or fullname matching text.

    search -- the query.
    skip -- the number of user to skip in the result (optional).
    limit -- the maximum number of match to return (optional).
    """
    with elle.log.trace('search %s (limit: %s, skip: %s)' % \
                        (search, limit, skip)):
      pipeline = []
      match = {}
      if not self.admin:
        # User must not be a ghost as ghost fullnames are their email
        # addresses.
        match['register_status'] = 'ok'
      if ids is not None:
        for c in "'\"[]":
          ids = ids.replace(c, '')
        ids = [self.__object_id(id) for id in ids.split(',')]
        match['_id'] = {'$in' : list(ids)}
      if search is not None:
        match['$or'] = [
          {'fullname' : {'$regex' : search,  '$options': 'i'}},
          {'handle' : {'$regex' : search, '$options': 'i'}},
        ]
      pipeline.append({'$match': match})
      # Mongo 2.6 requires a project after a match with an or.
      fields = self.user_public_fields
      fields['swaggers'] = '$swaggers'
      pipeline.append({
        '$project': fields,
      })
      if self.logged_in:
        pipeline.append({
          '$sort': {'swaggers.%s' % str(self.user['_id']) : -1}
        })
      pipeline.append({'$skip': skip})
      pipeline.append({'$limit': limit})
      users = self.database.users.aggregate(pipeline)
      for user in users['result']:
        del user['swaggers']
      return {'users': users['result']}

  def __users_by_emails_search(self, emails, limit, offset):
    """Search users for a list of emails.

    emails -- list of emails to search with.
    limit -- the maximum number of results.
    offset -- number to skip in search.
    """
    with elle.log.trace("%s: search %s emails (limit: %s, offset: %s)" %
                        (self.user['_id'], len(emails), limit, offset)):
      ret_keys = dict(**self.user_public_fields)
      if 'email' not in ret_keys:
        ret_keys['email'] = '$email'
      res = self.database.users.aggregate([
        {
          '$match':
          {
            'email': {'$in': emails},
            'register_status': 'ok',
          }
        },
        {'$limit': limit},
        {'$skip': offset},
        {'$project': ret_keys}
      ])
      return {'users': res['result']}

  @api('/user/search_emails', method = 'POST')
  @require_logged_in
  def users_by_emails_search(self,
                             emails,
                             limit : int = 50,
                             offset : int = 0):
    return self.__users_by_emails_search(emails, limit, offset)

  # Backwards compatibility for < 0.9.9
  @api('/user/search_emails', method = 'GET')
  @require_logged_in
  def old_users_by_emails_search(self,
                                 emails : json_value = [],
                                 limit : int = 50,
                                 offset : int = 0):
    return self.__users_by_emails_search(emails, limit, offset)


  # Historically we used _id but we're moving to id. This function extracts
  # fields for both cases.
  def extract_user_fields(self, user):
    if '_id' in user.keys():
      user_id = user['_id']
    else:
      user_id = user['id']
    if self.admin:
      res = dict(user)
      for key in ['avatar', 'small_avatar']:
        if key in res.keys():
          del res[key]
        if 'public_key' not in res.keys():
          res['public_key'] = ''
        if 'fullname' not in res.keys():
          res['fullname'] = ''
        if 'handle' not in res.keys():
          res['handle'] = ''
        if 'connected_devices' not in res.keys():
          res['connected_devices'] = []
    else:
      res = {
        'public_key': user.get('public_key', ''),
        'fullname': user.get('fullname', ''),
        'handle': user.get('handle', ''),
        'connected_devices': user.get('connected_devices', []),
        'register_status': user.get('register_status'),
      }
    res.update({
      '_id': user_id, # Backwards compatibility.
      'id': user_id,
      'status': self._is_connected(user_id),
    })
    return res

  @api('/users/<id_or_email>')
  def view_user(self, id_or_email):
    """
    Get user's public information by user_id or email.
    """
    user = self.user_by_id_or_email(id_or_email)
    if user is None:
      return self.not_found()
    else:
      return self.extract_user_fields(user)

  # Required for version <= 0.9.14.
  @api('/user/<id_or_email>/view')
  def view_user_old(self, id_or_email):
    """
    Get user's public information by user_id or email.
    """
    user = self.user_by_id_or_email(id_or_email)
    if user is None:
      return self.fail(error.UNKNOWN_USER)
    else:
      return self.success(self.extract_user_fields(user))

  @api('/users/from_handle/<handle>')
  @require_logged_in
  def view_from_handle(self, handle):
    """
    Get user information from handle
    """
    with elle.log.trace("%s: search user from handle %s" % (self, handle)):
      user = self.user_by_handle(handle, ensure_existence = False)
      if user is None:
        return self.not_found()
      else:
        return self.extract_user_fields(user)

  # Required for version <= 0.9.14.
  @api('/user/from_handle/<handle>/view')
  @require_logged_in
  def view_from_handle_old(self, handle):
    """
    Get user information from handle
    """
    with elle.log.trace("%s: search user from handle %s" % (self, handle)):
      user = self.user_by_handle(handle, ensure_existence = False)
      if user is None:
        return self.fail(error.UNKNOWN_USER)
      else:
        return self.success(self.extract_user_fields(user))

  @api('/user/from_public_key')
  def view_from_publick_key(self, public_key):
    with elle.log.trace("%s: search user from public key %s" % (self, public_key)):
      user = self.user_by_public_key(public_key)
      return self.success(self.extract_user_fields(user))

  ## ------- ##
  ## Swagger ##
  ## ------- ##
  def _increase_swag(self, lhs, rhs):
    """Increase users reciprocal swag amount.

    lhs -- the first user.
    rhs -- the second user.
    """
    with elle.log.trace("increase %s and %s mutual swag" % (lhs, rhs)):
      assert isinstance(lhs, bson.ObjectId)
      assert isinstance(rhs, bson.ObjectId)

      # lh_user = self._user_by_id(lhs)
      # rh_user = self._user_by_id(rhs)

      # if lh_user is None or rh_user is None:
      #   raise Exception("unknown user")

      for user, peer in [(lhs, rhs), (rhs, lhs)]:
        res = self.database.users.find_and_modify(
          {'_id': user},
          {'$inc': {'swaggers.%s' % peer: 1}},
          new = True)
        if res['swaggers'][str(peer)] == 1: # New swagger.
          self.notifier.notify_some(
            notifier.NEW_SWAGGER,
            message = {'user_id': res['_id']},
            recipient_ids = {peer},
          )

  # Used up to 0.9.1 for fetching swaggers.
  @api('/user/swaggers')
  @require_logged_in
  def swaggers(self):
    user = self.user
    with elle.log.trace("%s: get his swaggers" % user['email']):
      return self.success({"swaggers" : list(user["swaggers"].keys())})

  # Replaces /user/swaggers as of 0.9.2.
  @api('/user/full_swaggers')
  @require_logged_in
  def full_swaggers(self):
    user = self.user
    swaggers = user['swaggers']
    query = {
      '_id': {
        '$in': list(map(bson.ObjectId, swaggers.keys()))
      }
    }
    res = (
      self.extract_user_fields(user)
      for user in self.database.users.aggregate([
          {'$match': query},
          {'$project': self.user_public_fields},
      ])['result'])
    return self.success({
      'swaggers': sorted(res, key = lambda u: swaggers[str(u['id'])]),
    })

  @api('/user/add_swagger', method = 'POST')
  @require_admin
  def add_swagger(self,
                  user1: bson.ObjectId,
                  user2: bson.ObjectId):
    """Make user1 and user2 swaggers.
    This function is reserved for admins.

    user1 -- one user.
    user2 -- the other user.
    admin_token -- the admin token.
    """
    with elle.log.trace('%s: increase swag' % self):
      self._increase_swag(user1, user2,)
      return self.success()

  @api('/user/remove_swagger', method = 'POST')
  @require_logged_in
  def remove_swagger(self,
                     _id: bson.ObjectId):
    """Remove a user from swaggers.

    _id -- the id of the user to remove.
    """
    user = self.user
    with elle.log.trace("%s: remove swagger %s" % (user['_id'], _id)):
      swagez = self.database.users.find_and_modify(
        {'_id': user['_id']},
        {'$pull': {'swaggers': _id}},
        True #upsert
      )
      return self.success()

  def _notify_swaggers(self,
                       notification_id,
                       data,
                       user_id = None):
    """Send a notification to each user swaggers.

    notification_id -- the id of the notification to send.
    data -- the body of the notification.
    user_id -- emiter of the notification (optional,
               if logged in source is the user)
    """
    if user_id is None:
      user_id = self.user['_id']
    else:
      assert isinstance(user_id, bson.ObjectId)
    user = self._user_by_id(user_id)
    swaggers = set(map(bson.ObjectId, user['swaggers'].keys()))
    d = {"user_id" : user_id}
    d.update(data)
    self.notifier.notify_some(
      notification_id,
      recipient_ids = swaggers,
      message = d,
    )

  ## ---------- ##
  ## Favortites ##
  ## ---------- ##

  @api('/user/favorite', method = 'POST')
  @require_logged_in
  def favorite(self,
               user_id: bson.ObjectId):
    """Add a user to favorites

    user_id -- the id of the user to add.
    """
    query = {'_id': self.user['_id']}
    update = { '$addToSet': { 'favorites': user_id } }
    self.database.users.update(query, update)
    return self.success()

  @api('/user/unfavorite', method = 'POST')
  @require_logged_in
  def unfavorite(self,
                 user_id: bson.ObjectId):
    """remove a user to favorites

    user_id -- the id of the user to add.
    """
    query = {'_id': self.user['_id']}
    update = { '$pull': { 'favorites': user_id } }
    self.database.users.update(query, update)
    return self.success()

  ## ---- ##
  ## Edit ##
  ## ---- ##

  @api('/user/edit', method = 'POST')
  @require_logged_in
  def edit(self,
           fullname,
           handle):
    """ Edit fullname and handle.

    fullname -- the new user fullname.
    hadnle -- the new user handle.
    """
    user = self.user
    handle = handle.strip()
    # Clean the forbidden char from asked handle.
    handle = self.__generate_handle(handle, enlarge = False)
    fullname = fullname.strip()
    lw_handle = handle.lower()
    if not len(fullname) > 2:
      return self.fail(
        error.OPERATION_NOT_PERMITTED,
        "Fullname is too short",
        field = 'fullname',
        )
    if not len(lw_handle) > 2:
      return self.fail(
        error.OPERATION_NOT_PERMITTED,
        "Handle is too short",
        field = 'handle',
        )
    other = self.database.users.find_one({'lw_handle': lw_handle})
    if other is not None and other['_id'] != user['_id']:
      return self.fail(
        error.HANDLE_ALREADY_REGISTERED,
        field = 'handle',
        )
    update = {
      '$set': {'handle': handle, 'lw_handle': lw_handle, 'fullname': fullname}
    }
    self.database.users.find_and_modify(
      {'_id': user['_id']},
      update)
    return self.success()

  @api('/user/invite', method = 'POST')
  @require_logged_in
  def invite(self, email):
    """Invite a user to infinit.
    This function is reserved for admins.

    email -- the email of the user to invite.
    admin_token -- the admin token.
    """
    user = self.user
    with elle.log.trace("%s: invite %s" % (user['email'], email)):
      if regexp.EmailValidator(email) != 0:
        return self.fail(error.EMAIL_NOT_VALID)
      if self.database.users.find_one({"email": email}) is not None:
        self.fail(error.USER_ALREADY_INVITED)
      invitation.invite_user(
        email = email,
        send_email = True,
        mailer = self.mailer,
        source = (user['fullname'], user['email']),
        database = self.database,
        merge_vars = {
          email: {
            'sendername': user['fullname'],
            'user_id': str(user['_id']),
          }}
      )
      return self.success()

  @api('/user/invited')
  @require_logged_in
  def invited(self):
    """Return the list of users invited.
    """
    return self.success({'user': list(map(lambda u: u['email'], self.database.invitations.find(
        {
          'source': self.user['email'],
        },
        fields = {'email': True, '_id': False}
    )))})

  def _user_self(self):
    user = self.user
    res = {
      '_id': user['_id'], # Used until 0.9.9
      'id': user['_id'],
      'fullname': user['fullname'],
      'handle': user['handle'],
      'register_status': user['register_status'],
      'email': user['email'],
      'devices': user.get('devices', []),
      'networks': user.get('networks', []),
      'identity': user['identity'],
      'public_key': user['public_key'],
      'accounts': user['accounts'],
      'remaining_invitations': user.get('remaining_invitations', 0),
      'token_generation_key': user.get('token_generation_key', ''),
      'favorites': user.get('favorites', []),
      'connected_devices': user.get('connected_devices', []),
      'status': self._is_connected(user['_id']),
      'creation_time': user.get('creation_time', None),
      'last_connection': user.get('last_connection', 0),
    }
    if not user.get('email_confirmed', True):
      from time import time
      res.update({
        'unconfirmed_email_leeway': user['unconfirmed_email_deadline'] - time()
      })
    return res

  @api('/user/self')
  @require_logged_in
  def user_self(self):
    """Return self data."""
    return self.success(self._user_self())

  @api('/user/minimum_self')
  @require_logged_in
  def minimum_self(self):
    """Return minimum self data.
    """
    user = self.user
    return self.success(
      {
        'email': user['email'],
        'identity': user['identity'],
      })

  @api('/user/remaining_invitations')
  @require_logged_in
  def invitations(self):
    """Return the number of invitations remainings.
    """
    return self.success(
      {
        'remaining_invitations': self.user.get('remaining_invitations', 0),
      })

  def user_avatar_route(self, id):
    from bottle import Request
    return Request().url + 'user/%s/avatar' % str(id)

  @api('/user/<id>/avatar')
  def get_avatar(self,
                 id: bson.ObjectId,
                 date: int = 0,
                 no_place_holder: bool = False):
    user = self._user_by_id(id, ensure_existence = False)
    if user is None:
      if no_place_holder:
        return self.not_found()
      else: # Return the default avatar for backwards compatibility (< 0.9.2).
        from bottle import static_file
        return static_file('place_holder_avatar.png',
                           root = os.path.dirname(__file__),
                           mimetype = 'image/png')

    small_image = user.get('small_avatar')
    if small_image:
      from bottle import response
      response.content_type = 'image/png'
      return bytes(small_image)
    else:
      image = user.get('avatar')
      if image:
        from io import BytesIO
        from PIL import Image
        image_data = BytesIO(bytes(image))
        image_data.seek(0)
        small_image = self._small_avatar(Image.open(image_data))
        small_out = BytesIO()
        small_image.save(small_out, 'PNG')
        small_out.seek(0)
        res = self.database.users.find_and_modify(
          {'_id': user['_id']},
          {'$set': {
            'small_avatar': bson.binary.Binary(small_out.read()),
          }},
          new = True,
          fields = {'small_avatar': True, '_id': False})['small_avatar']
        from bottle import response
        response.content_type = 'image/png'
        return bytes(res)
      else:
        if no_place_holder:
          return self.not_found()
        else: # Return the default avatar for backwards compatibility (< 0.9.2).
          from bottle import static_file
          return static_file('place_holder_avatar.png',
                             root = os.path.dirname(__file__),
                             mimetype = 'image/png')

  def _small_avatar(self, avatar):
    from PIL import Image
    small_avatar = avatar.resize((256, 256), Image.ANTIALIAS)
    return small_avatar

  @api('/user/avatar', method = 'POST')
  @require_logged_in
  def set_avatar(self):
    from bottle import request
    from io import BytesIO
    from PIL import Image
    image = Image.open(request.body)
    small_image = self._small_avatar(image)
    out = BytesIO()
    small_out = BytesIO()
    image.save(out, 'PNG')
    small_image.save(small_out, 'PNG')
    out.seek(0)
    small_out.seek(0)
    import bson.binary
    res = self.database.users.find_and_modify(
      query = {'_id': self.user['_id']},
      update = {'$set': {
        'avatar': bson.binary.Binary(out.read()),
        'small_avatar': bson.binary.Binary(small_out.read()),
      }})
    return self.success()

  ## ----------------- ##
  ## Connection status ##
  ## ----------------- ##
  def set_connection_status(self,
                            user_id,
                            device_id,
                            status):
    """Add or remove the device from user connected devices.

    device_id -- the id of the requested device
    user_id -- the device owner id
    status -- the new device status
    """
    with elle.log.trace("%s: %sconnected on device %s" %
                        (user_id, not status and "dis" or "", device_id)):
      assert isinstance(user_id, bson.ObjectId)
      assert isinstance(device_id, uuid.UUID)
      user = self.database.users.find_one({"_id": user_id})
      assert user is not None
      device = self.device(id = str(device_id), owner = user_id)
      assert str(device_id) in user['devices']
      update_action = status and '$addToSet' or '$pull'
      action = {update_action: {'connected_devices': str(device_id)}}
      # If we're connecting a device, then we are connected.
      if status:
        action['$set'] = {
          'connected': True,
          'connection_time': self.now,
        }
      self.database.users.update(
        {'_id': user_id},
        action,
        multi = False,
      )
      # If we're disconnecting a device, use DB to determine if we're offline.
      if not status:
        self.database.users.update(
          {
            '_id': user_id,
            'connected_devices': {'$size': 0}
          },
          {
            '$set':
            {
              'connected': False,
              'disconnection_time': self.now,
            }
          }
        )
      # XXX:
      # This should not be in user.py, but it's the only place
      # we know the device has been disconnected.
      if status is False:
        with elle.log.trace("%s: disconnect nodes" % user_id):
          transactions = self.find_nodes(user_id = user['_id'],
                                         device_id = device_id)

          with elle.log.debug("%s: concerned transactions:" % user_id):
            for transaction in transactions:
              elle.log.debug("%s" % transaction)
              self.update_node(transaction_id = transaction['_id'],
                               user_id = user['_id'],
                               device_id = device_id,
                               node = None)
              self.notifier.notify_some(
                notifier.PEER_CONNECTION_UPDATE,
                recipient_ids = {transaction['sender_id'], transaction['recipient_id']},
                message = {
                  "transaction_id": str(transaction['_id']),
                  "devices": [transaction['sender_device_id'], transaction['recipient_device_id']],
                  "status": False
                }
              )

      self._notify_swaggers(
        notifier.USER_STATUS,
        {
          'status': self._is_connected(user_id),
          'device_id': str(device_id),
          'device_status': status,
        },
        user_id = user_id,
      )

  # Email subscription.
  # XXX: Make it a decorator.

  def user_unsubscription_hash(self, user):
    """
    Return the hash that will be used to find the user (compute it if absent).
    This hash will be used to manage email subscriptions.

    user -- The user.
    """
    with elle.log.debug('get user hash for email'):
      assert user is not None
      if not hasattr(user, 'email_hash'):
        import hashlib
        hash = hashlib.md5(str(user['_id']).encode('utf-8')).hexdigest()
        user = self.database.users.find_and_modify(
          {"email": user['email']},
          {"$set": {'email_hash': hash}},
          new = True)
    return user['email_hash']

  def __user_by_email_hash(self, hash):
    """
    Return the user linked the hash.
    """
    with elle.log.debug('get user from email hash'):
      user = self.database.users.find_one({'email_hash': hash})
      if user is None:
        raise error.Error(
          error.UNKNOWN_USER,
        )
      return user


  def __subscriptions(self, user):
    """
    Return the status of every subscriptions for a specified user.

    user -- The user.
    """
    unsubscribed = user.get('unsubscriptions', [])
    return {
      k:
      {
        'status': not k in unsubscribed,
        'pretty': mail.subscriptions[k]
      } for k in mail.subscriptions.keys()
    }

  @api('/user/email_subscriptions', method = 'GET')
  @require_logged_in
  def mail_subscriptions(self):
    """
    Return the status of every subscriptions.
    """
    user = self.user
    return self.success({"subscriptions": self.__subscriptions(user)})

  def has_email_subscription(self, user, name):
    subscription = mail.subscription_name(name)
    return subscription not in user.get('unsubscriptions', [])

  @api('/user/email_subscription/<name>', method = 'GET')
  @require_logged_in
  def get_mail_subscription(self, name):
    """
    Return the status for a specific subscription.

    type -- The name of the subscription.
    """
    try:
      user = self.user
      return self.success({name: self.has_email_subscription(user, name)})
    except mail.EmailSubscriptionNotFound as e:
      self.not_found()

  def __modify_subscription(self, user, name, value):
    """
    Add or remove a subscription for a user.

    user -- The user to edit.
    name -- The name of the subscription to edit.
    value -- The status wanted for the subscription.
    """
    action = value and 'subscribe' or 'unsubscribe'
    with elle.log.debug(
        '%s %s from %s emails' % (action, user['email'], name)):
      try:
        name = mail.subscription_name(name)
        action = value and "$pop" or "$addToSet"
        document = { action: {"unsubscriptions": name} }
        self.database.users.update({'_id': user['_id']},
                                   document = document,
                                   upsert = False)
        return {name: value}
      except mail.EmailSubscriptionNotFound as e:
        self.not_found()


  def __set_subscription(self, user, unsubscriptions):
    """
    Add or remove a subscription for a user.

    user -- The user to edit.
    name -- The name of the subscription to edit.
    value -- The status wanted for the subscription.
    """
    document = { "$set": { "unsubscriptions": unsubscriptions } }
    self.database.users.update({'_id': user['_id']},
                               document = document,
                               upsert = False)

  @api('/user/email_subscription/<name>', method = 'DELETE')
  @require_logged_in
  def delete_mail_subscription_logged(self, name):
    """
    Remove a specific subscription.

    name -- The name of the subscription to edit.
    """
    return self.__modify_subscription(self.user, name, False)

  @api('/user/<hash>/email_subscription/<name>', method = 'DELETE')
  def delete_mail_subscription(self, hash, name):
    """
    Restore a specify subscription.

    hash -- The hash associated to the user.
    name -- The name of the subscription to edit.
    """
    user = self.__user_by_email_hash(hash)
    return self.__modify_subscription(user, name, False)

  @api('/users/<user>/email_subscriptions/<name>', method = 'PUT')
  @require_key
  def mail_subscribe_key(self, user, name):
    """
    Subscribe to an email set.

    user -- Email or id of the user to unsubscribe.
    name -- Name of the email set.
    """
    user = self.user_by_id_or_email(user)
    return self.__modify_subscription(user, name, True)

  @api('/users/<user>/email_subscriptions/<name>', method = 'DELETE')
  @require_key
  def mail_unsubscribe_key(self, user, name):
    """
    Unsubscribe from an email set.

    user -- Email or id of the user to unsubscribe.
    name -- Name of the email set.
    """
    user = self.user_by_id_or_email(user)
    return self.__modify_subscription(user, name, False)

  # Restore.
  @api('/user/email_subscription/<name>', method = 'PUT')
  @require_logged_in
  def restore_mail_subscription(self, name):
    """
    Restore a specific subscription.

    name -- The name of the subscription to edit.
    """
    return self.__modify_subscription(self.user, name, True)

  @api('/user/email_subscriptions', method = 'POST')
  @require_logged_in
  def change_mail_subscriptions(self,
                                subscriptions):
    try:
      user = self.user
      if subscriptions is None:
        subscriptions = []
      subscriptions = isinstance(subscriptions, (str)) and [subscriptions] or subscriptions
      subscriptions = list(map(lambda x: mail.subscription_name(str(x)), subscriptions))
      self.__set_subscription(user, [x for x in mail.subscriptions.keys() \
                                     if x not in subscriptions])
      return self.success()
    except mail.EmailSubscriptionNotFound as e:
        self.not_found()

  ## --------- ##
  ## Campaigns ##
  ## --------- ##
  @api('/users/campaign/<campaign>')
  @require_admin
  def users_from_campaign(self, campaign):
    with elle.log.debug('users by campaign: %s' % campaign):
      users = self.database.users.find(
        {'source': campaign},
        fields = {
          '_id': False,
          'email': True,
          'fullname': True,
        })
      res = list()
      for user in users:
        res.append(user)
      return {'users': res}

  ## ----- ##
  ## Debug ##
  ## ----- ##

  @api('/debug', method = 'POST')
  @require_logged_in
  def message(self,
              sender_id: bson.ObjectId,
              recipient_id: bson.ObjectId,
              message):
    """Send a message to recipient as sender.

    sender_id -- the id of the sender.
    recipient_id -- the id of the recipient.
    message -- the message to be sent.
    """
    self.notifier.notify_some(
      notifier.MESSAGE,
      recipient_ids = {recipient_id},
      message = {
        'sender_id' : sender_id,
        'message': message,
      }
    )
    return self.success()
