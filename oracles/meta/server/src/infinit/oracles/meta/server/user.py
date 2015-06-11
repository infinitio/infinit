# -*- encoding: utf-8 -*-

import bottle
import bson
import collections
import datetime
import json
import bson.code
import random
import uuid
import re
import stripe

import elle.log
import papier
import infinit.oracles.emailer

from .plugins.response import response, Response
from .utils import api, require_logged_in, require_logged_in_fields, require_admin, require_logged_in_or_admin, hash_password, json_value, require_key, key, clean_up_phone_number
from . import utils
from . import error, notifier, regexp, conf, invitation, mail, transaction_status

import pymongo
import pymongo.errors
from pymongo import DESCENDING
import os
import string
import time
import unicodedata

code_alphabet = '2346789abcdefghilkmnpqrstuvwxyz'
code_length = 5

#
# Users
#
ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.User'

class Mixin:

  def __user_fill(self, user):
    '''Fill dynamic fields for users fetched from the database.'''
    if user is None:
      return user
    if 'devices' in user:
      connected = [d['id']
                   for d in user['devices'] if d.get('trophonius')]
      user['connected_devices'] = connected
      user['connected'] = bool(connected)
    return user

  def __user_view(self, user):
    '''Layout user to be returned to clients'''
    user = self.__user_self(user)
    # Devices are fetched to compute connectivity, hide them
    del user['devices']
    # FIXME: Here's the joke: we return ghost and registered users
    # mixed, and the client expects public_key and handle strings. Fix
    # this.
    if 'public_key' not in user:
      user['public_key'] = ''
    if 'handle' not in user:
      user['handle'] = ''
    if user['register_status'] == 'ghost':
      # Only the user with a ghost code are concerned (>= 0.9.31).
      if 'ghost_code' in user:
        if 'phone_number' not in user:
          del user['ghost_code']
        else:
          user['ghost_profile'] = user.get(
            'shorten_ghost_profile_url',
            self.__ghost_profile_url(user, type = 'phone'))
        if 'shorten_ghost_profile_url' in user:
          del user['shorten_ghost_profile_url']
    return user

  def __user_self(self, user):
    '''Layout self-user to be returned to clients'''
    if user is None:
      return user
    user['id'] = user['_id']
    del user['_id']
    user['status'] = user['connected'] # FIXME: seriously WTF
    user['devices'] = [d['id'] for d in user['devices']]
    if 'favorites' not in user:
      user['favorites'] = []
    if self.stripe_api_key is not None and 'stripe_id' in user:
      cus = stripe.Customer.retrieve(
        user['stripe_id'],
        expand = ['subscriptions'],
        api_key = self.stripe_api_key,
      )
      if cus.subscriptions.total_count > 0:
          user['subscription_data'] = cus.subscriptions.data[0]
    return user

  @property
  def __user_view_fields(self):
    res = [
      '_id',
      # Fetch devices so __user_fill can compute connectivity
      'devices.id',
      'devices.trophonius',
      'devices.country_code',
      'fullname',
      'handle',
      'public_key',
      'register_status',
      'facebook_id',
      'phone_number',
      'ghost_code',
      'shorten_ghost_profile_url',
    ]
    if self.admin:
      res += [
        'creation_time',
        'email',
        'email_confirmed',
        'features',
        'os',
        'plan',
        'quota',
      ]
    return res

  @property
  def __user_self_fields(self):
    res = self.__user_view_fields
    res += [
      'accounts',
      'creation_time',
      'email',
      'facebook_id',
      'favorites',
      'features',
      'identity',
      'swaggers',
      'plan',
      'quota',
      'consumed_ghost_codes',
      'stripe_id',
    ]
    return res

  def __user_fetch(self, query, fields = None):
    return self.__user_fill(
      self.database.users.find_one(query, fields = fields))

  def __user_fetch_and_modify(self, query, update, fields, new):
    return self.__user_fill(self.database.users.find_and_modify(
      query,
      update,
      fields = fields,
      new = new,
    ))

  def __users_fetch(self, query,
                    fields = None,
                    limit = None,
                    skip = None):
    users = self.database.users.find(query, fields = fields)
    if skip is not None:
      users = users.skip(skip)
    if limit is not None:
      users = users.limit(limit)
    return [self.__user_fill(u) for u in users]

  def __users_count(self, query, fields = None):
    return self.database.users.find(query, fields = ['_id']).count()

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
    while self.user_by_handle(h, fields = [],
                              ensure_existence = False):
      h += str(int(random.random() * 10))
    return h

  ## -------- ##
  ## Sessions ##
  ## -------- ##

  def user_identifier(self, user):
    # Because a big part of the system was based on the fact that an user
    # always have an email address, facebook users are problematic because
    # it's not mandatory.
    res = user.get('email')
    if res is None:
      res = user.get('facebook_id')
    if res is None:
      res = user.get('fullname')
    assert len(res)
    return res

  def _forbidden_with_error(self, error):
    ret_msg = {'code': error[0], 'message': error[1]}
    response(403, ret_msg)

  def remove_current_session(self, user = None, device = None):
    for key in ['identifier', 'email']:
      if key in bottle.request.session:
        # Web sessions have no device.
        if 'device' in bottle.request.session:
          self.database.users.update(
            {'devices.id': bottle.request.session['device']},
            {'$unset': {'devices.$.push_token': True}},
          )
          del bottle.request.session['device']
        if 'facebook_access_token' in bottle.request.session:
          del bottle.request.session['facebook_access_token']
        del bottle.request.session[key]
        return True
    return False

  def remove_session(self, user, device = None):
    query = ({'identifier': {'$in': [user.get('email'), user['_id']]}})
    if device:
      query.update({'device': device['id']})
    self.sessions.remove(query)

  def _login(self,
             email: utils.enforce_as_email_address,
             fields,
             password,
             password_hash = None):
    try:
      user = self.user_by_email_password(
        email, password = password, password_hash = password_hash,
        fields = fields + ['email_confirmed',
                           'unconfirmed_email_deadline'],
        ensure_existence = True)
      if not user['email_confirmed']:
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
      return {'self': self.__user_self(user)}
    else:
      res = {
        '_id' : user['_id'],
        'fullname': user['fullname'],
        'email': user.get('email', ''),
        'handle': user['handle'],
        'register_status': user['register_status'],
      }
      if not web:
        assert device is not None
        res.update({
          'identity': user['identity'],
          'device_id': device['id'],
        })
      return res

  def _in_app_login(self,
                    user,
                    device_id,
                    password,
                    OS = None,
                    pick_trophonius = None,
                    device_push_token: str = None,
                    country_code = None,
                    device_name = None,
                    device_model = None,
                    device_language = None,
  ):
    # If creation process was interrupted, generate identity now.
    if 'public_key' not in user:
      user = self.__generate_identity(user, password)
    query = {'id': str(device_id), 'owner': user['_id']}
    if device_push_token is None:
      login_update = {
        '$unset':
        {
          'devices.$.push_token': True,
        },
        '$set':
        {
          'devices.$.country_code': country_code,
        }
      }
    else:
      login_update = {
        '$set':
        {
          'devices.$.push_token': device_push_token,
          'devices.$.country_code': country_code,
        }
      }
    def login():
      return self.database.users.find_and_modify(
        {'_id': user['_id'], 'devices.id': str(device_id)},
        login_update,
        fields = ['devices']
      )
    usr = self.device_override_push_token(device_push_token, login)
    if usr is None:
      elle.log.trace("user logged with an unknown device")
      device = self._create_device(
        id = device_id,
        name = device_name,
        owner = user,
        device_push_token = device_push_token,
        OS = OS,
        country_code = country_code,
        device_model = device_model)
    else:
      device = list(filter(lambda x: x['id'] == str(device_id), usr['devices']))[0]
    # Remove potential leaked previous session.
    self.remove_session(user, device)
    elle.log.debug("%s: store session" % user['_id'])

    # Check if this is the first login of this user
    res = self.database.users.find_one(
      {
        '_id': user['_id'],
        'last_connection': {'$exists': False},
      },
      fields = ['referred_by'])
    if res is not None and 'referred_by' in res:
      self.process_referrals(user, res['referred_by'])

    bottle.request.session['device'] = device['id']
    bottle.request.session['identifier'] = user['_id']

    self.database.users.update(
      {'_id': user['_id']},
      {'$set': {'last_connection': time.time(),}})
    elle.log.trace("successfully connected as %s on device %s" %
                   (user['_id'], device['id']))
    if OS is not None:
      OS = OS.strip().lower()
    if 'email' in user:
      email = user['email']
      if OS is not None and OS in invitation.os_lists.keys() and ('os' not in user or OS not in user['os']):
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
        elle.log.debug("%s: no OS specified" % user['_id'])
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
    # Force immediate buffering on mobile devices.
    if self.device_mobile:
      features['preemptive_buffering_delay'] = '0'
    response['features'] = list(features.items())
    response['device'] = self.device_view(device)
    return response

  @api('/users/facebook/<facebook_id>')
  def is_registered_with_facebook(self, facebook_id):
    user = self.user_by_facebook_id(facebook_id,
                                    fields = [], # Only the id.
                                    ensure_existence = False)
    if user is not None:
      return {}
    else:
      return self.not_found({
        'reason': 'unknown facebook id %s' % facebook_id
      })

  def __facebook_connect(self,
                         fields,
                         short_lived_access_token = None,
                         long_lived_access_token = None,
                         preferred_email: utils.enforce_as_email_address = None,
                         source = None):
    registered = False
    if bool(short_lived_access_token) == bool(long_lived_access_token):
      return self.bad_request({
        'reason': 'you must provide short or long lived token'
      })
    try:
      facebook_user = self.facebook.user(
        short_lived_access_token = short_lived_access_token,
        long_lived_access_token = long_lived_access_token)
      user = self.user_by_facebook_id(facebook_user.facebook_id,
                                      fields = fields,
                                      ensure_existence = False)
      new = False
      if user is None: # Register the user.
        new = True
        user = self.facebook_register(
          facebook_user = facebook_user,
          preferred_email = preferred_email,
          source = source,
          fields = fields)
        registered = True
        try:
          self._set_avatar(user, facebook_user.avatar)
        except BaseException as e:
          elle.log.warn('unable to get facebook avatar: %s' % e)
          pass
      # Set a 0 swaggers with us to our facebook friends
      f = facebook_user.friends
      friend_ids = [friend['id'] for friend in f['data']]
      self.database.users.update(
        {
          'accounts.id' : {'$in': friend_ids},
          'swaggers.' + str(user['_id']) : {'$exists': False}
        },
        {
         '$inc': {'swaggers.' + str(user['_id']) : 0}
        },
        multi = True
        )
      return user, registered
    except Response as r:
      raise r
    except Exception as e:
      self._forbidden_with_error(e.args[0])
    except error.Error as e:
      self._forbidden_with_error(e.args[0])

  @api('/login', method = 'POST')
  def login(self,
            device_id: uuid.UUID,
            email: utils.enforce_as_email_address = None,
            password: str = None,
            short_lived_access_token: str = None,
            long_lived_access_token: str = None,
            preferred_email: str = None,
            password_hash: str = None,
            OS: str = None,
            device_push_token = None,
            country_code = None,
            pick_trophonius: bool = True,
            device_model : str = None,
            device_name : str = None,
            device_language: str = None):
    # Check for service availability
    # XXX TODO: Fetch maintenance mode bool from somewhere
    maintenance_mode = False

    if maintenance_mode:
      return self.unavailable({
        'reason': 'Server is down for maintenance.',
        'code': error.MAINTENANCE_MODE
        })

    # Xor facebook_token or email / password.
    with_email = bool(email is not None and password is not None)
    with_facebook = bool(long_lived_access_token is not None or
                         short_lived_access_token is not None)
    if with_email == with_facebook:
      return self.bad_request({
        'reason': 'you must provide facebook_token or (email, password)'
      })
    # FIXME: 0.0.0.0 is the website.
    if self.user_version < (0, 9, 0) and self.user_version != (0, 0, 0):
      return self.fail(error.DEPRECATED)
    fields = self.__user_self_fields + ['public_key']
    with elle.log.trace("%s: log on device %s" % (email or 'facebook user', device_id)):
      registered = False
      if with_email:
        user = self._login(email = email,
                           fields = fields,
                           password = password,
                           password_hash = password_hash)
      elif with_facebook:
        user, registered = self.__facebook_connect(
          short_lived_access_token = short_lived_access_token,
          long_lived_access_token = long_lived_access_token,
          preferred_email = preferred_email,
          fields = fields)
      res = self._in_app_login(user = user,
                               password = password,
                               device_id = device_id,
                               OS = OS,
                               pick_trophonius = pick_trophonius,
                               device_push_token = device_push_token,
                               country_code = country_code,
                               device_model = device_model,
                               device_name = device_name,
                               device_language = device_language)
      res.update({'account_registered': registered})
      if registered:
        ghost_codes = user.get('consumed_ghost_codes', [])
        if len(ghost_codes):
          res.update({'ghost_code': ghost_codes[0]})
      return res

  @api('/web-login', method = 'POST')
  def web_login(self,
                email: utils.enforce_as_email_address = None,
                password = None,
                preferred_email: utils.enforce_as_email_address = None,
                short_lived_access_token = None,
                long_lived_access_token = None):
    # Xor facebook_token or email / password.
    with_email = bool(email is not None and password is not None)
    with_facebook = bool(long_lived_access_token is not None or
                         short_lived_access_token is not None)
    if with_email == with_facebook:
      return self.bad_request({
        'reason': 'you must provide facebook_token or (email, password)'
      })
    fields = self.__user_self_fields + ['unconfirmed_email_deadline']
    update = {'account_registered': False}
    if with_email:
      with elle.log.trace("%s: web login" % email):
        user = self._login(email, password = password, fields = fields)
    elif with_facebook:
      user, update['registered'] = self.__facebook_connect(
        short_lived_access_token = short_lived_access_token,
        long_lived_access_token = long_lived_access_token,
        preferred_email = preferred_email,
        fields = fields)
      ghost_codes = user.get('consumed_ghost_codes', [])
      if len(ghost_codes):
        update.update({'ghost_code': ghost_codes[0]})
    res = self._web_login(user)
    res.update(update)
    return res

  def _web_login(self, user):
    bottle.request.session['identifier'] = user['_id']
    user = self.user
    elle.log.trace("%s: successfully connected as %s" %
                   (self.user_identifier(user), user['_id']))
    return self.success(self._login_response(user, web = True))

  @api('/logout', method = 'POST')
  @require_logged_in
  def logout(self):
    user = self.user
    with elle.log.trace("%s: logout" % user['_id']):
      if self.remove_current_session():
        return self.success()
      return self.fail(error.NOT_LOGGED_IN)

  def kickout(self, reason, user = None):
    if user is None:
      user = self.user
    with elle.log.trace('kickout %s: %s' % (user['_id'], reason)):
      self.remove_session(user)
      self.notifier.notify_some(
        notifier.INVALID_CREDENTIALS,
        recipient_ids = {user['_id']},
        message = {'response_details': reason})

  @property
  def user(self):
    return self._user_from_session(fields = self.__user_self_fields)

  def _user_from_session(self, fields):
    if hasattr(bottle.request, 'user'):
      elle.log.trace('get user from session: cached')
      return bottle.request.user
    if not hasattr(bottle.request, 'session'):
      elle.log.trace('get user from session: no session')
      return None
    # For a smoother transition, sessions registered as
    # email are still available.
    methods = {'identifier': self._user_by_id, 'email': self.user_by_email}
    with elle.log.trace('get user from session'):
      elle.log.dump('session: %s' % bottle.request.session)
      for key in methods:
        identifier = bottle.request.session.get(key, None)
        if identifier is not None:
          elle.log.debug('get user from session by %s: %s' % (key, identifier))
          user = methods[key](identifier,
                              ensure_existence = False,
                              fields = fields)
          if user is not None:
            bottle.request.user = user
            return user
      elle.log.trace('session not found')

  ## -------- ##
  ## Register ##
  ## -------- ##

  def _register(self, **kwargs):
    user = self.database.users.save(kwargs)
    return user

  @api('/user/register', method = 'POST')
  def user_register_api(self,
                        email: utils.enforce_as_email_address,
                        password,
                        fullname,
                        source = None,
                        activation_code = None,
                        password_hash = None):
    if self.user is not None:
      return self.fail(error.ALREADY_LOGGED_IN)
    if password is None:
      return self.bad_request({
        'reason': 'Password field cannot be null',
      })
    try:
      user = self.user_register(email = email,
                                password = password,
                                fullname = fullname,
                                source = source,
                                activation_code = activation_code,
                                password_hash = password_hash)
      res = {
        'registered_user_id': user['id'],
        'invitation_source': '',
        'unconfirmed_email_leeway': self.unconfirmed_email_leeway,
      }
      ghost_codes = user.get('consumed_ghost_codes', [])
      if len(ghost_codes):
        res.update({'ghost_code': ghost_codes[0]})
      return self.success(res)
    except Exception as e:
      return self.fail(e.args[0])

  # facebook_register is a kind a clone of the register function except some
  # attributes aren't usefull:
  def facebook_register(self,
                        facebook_user,
                        fields,
                        preferred_email = None,
                        source = None):
    # XXX: Make that cleaner...
    email = facebook_user.data.get('email', None)
    if email is not None:
      email = utils.enforce_as_email_address(email)
    if preferred_email is not None:
      preferred_email = utils.enforce_as_email_address(preferred_email)
    # Confirmed by facebook.
    already_confirmed = preferred_email == email or preferred_email is None
    email = preferred_email or email
    if email is None:
      return self.bad_request({
        'reason': 'you must provide an email',
        'code': error.EMAIL_NOT_VALID[0]
      })
    name = facebook_user.data['name']
    extra_fields = {
      'facebook': {},
      'accounts': [{
        'type': 'facebook',
        'id': facebook_user.facebook_id
      }],
      'facebook_id': facebook_user.facebook_id,
    }
    for field in ['first_name', 'last_name', 'gender', 'timezone', 'locale', 'birthday']:
      if field in facebook_user.data:
        extra_fields['facebook'][field] = facebook_user.data[field]
    self.user_register(
      email = email,
      password = None,
      fullname = name,
      password_hash = None,
      source = source,
      email_is_already_confirmed = already_confirmed,
      extra_fields = extra_fields,
      activation_code = None)
    return self.user_by_facebook_id(
      facebook_user.facebook_id, fields = fields)

  def __email_confirmation_fields(self,
                                  email : utils.enforce_as_email_address):
    email = email.strip().lower()
    import hashlib
    hash = str(time.time()) + email
    hash = hash.encode('utf-8')
    hash = hashlib.md5(hash).hexdigest()
    return {
      'email_confirmed': False,
      'unconfirmed_email_deadline':
        time.time() + self.unconfirmed_email_leeway,
      'email_confirmation_hash': str(hash)
    }

  def user_register(self,
                    email: utils.enforce_as_email_address,
                    password,
                    fullname,
                    password_hash = None,
                    source = None,
                    email_is_already_confirmed = False,
                    extra_fields = None,
                    activation_code = None):
    """Register a new user.

    email -- the account email.
    password -- the client side hashed password.
    fullname -- the user fullname.
    activation_code -- the activation code.
    """
    _validators = [
      (fullname, regexp.FullnameValidator),
    ]
    if password is not None:
      _validators.append((password, regexp.PasswordValidator))
    for arg, validator in _validators:
      res = validator(arg)
      if res != 0:
        raise Exception(res)
    fullname = fullname.strip()
    with elle.log.trace("registration: %s as %s" % (email, fullname)):
      handle = self.unique_handle(fullname)
      plan = self.database.plans.find_one({'name': 'basic'})
      features = self._roll_features(True)
      quota = {}
      if plan is not None:
        features.update(plan.get('features', {}))
        quota = plan.get('quota', {})
      user_content = {
        'features': features,
        'register_status': 'ok',
        'email': email,
        'fullname': fullname,
        'password': hash_password(password),
        'handle': handle,
        'lw_handle': handle.lower(),
        'swaggers': {},
        'networks': [],
        'devices': [],
        'notifications': [],
        'old_notifications': [],
        'accounts': [collections.OrderedDict((('id', email), ('type', 'email')))],
        'creation_time': self.now,
        'plan': 'basic',
        'quota': quota,
      }
      if email_is_already_confirmed:
        user_content.update({'email_confirmed': True})
      else:
        user_content.update(self.__email_confirmation_fields(email))
      if extra_fields:
        if 'accounts' in extra_fields:
          user_content['accounts'] += extra_fields['accounts']
          del extra_fields['accounts']
        user_content.update(extra_fields)
      if password_hash is not None:
        user_content.update(
          {
            'password_hash': utils.password_hash(password_hash)
          })
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
        if user['register_status'] in ['ghost', 'contact']:
          for field in ['swaggers', 'features']:
            user_content[field] = user[field]
          update = {
            '$set': user_content,
          }
          if 'ghost_code' in user:
            update.setdefault('$unset', {}).update({
              'ghost_code': 1,
              'shorten_ghost_profile_url': 1,
              'ghost_code_expiration': 1,
            })
            update.setdefault('$addToSet', {}).update({
              'consumed_ghost_codes': user['ghost_code']
            })
          if 'phone_number' in user:
            update.setdefault('$unset', {}).update({
              'phone_number': 1})
          user = self.database.users.find_and_modify(
            query = {
              'accounts.id': email,
              'register_status': {'$in': ['ghost', 'contact']},
            },
            update = update,
            new = True,
            upsert = False,
          )
          if user is None:
            # The ghost was already transformed - prevent the race
            # condition.
            raise Exception(error.EMAIL_ALREADY_REGISTERED)
          # notify users that have us in contacts
          contact_of = user.get('contact_of', [])
          if len(contact_of):
            self.database.users.update({'_id': {'$in': contact_of}},
              {'$inc': {'swaggers.' + str(user['_id']): 0}
              }, multi = True)
          for c in contact_of:
            self.notifier.notify_some(
              notifier.NEW_SWAGGER,
              message = {
                'user_id': str(user['_id']),
                'contact': email,
              },
              recipient_ids = {c},
              )
          # Mark invitations as completed
          self.database.invitations.update(
            {'recipient': user['_id']},
            {'$set': {'status': 'completed'}},
            multi = True)
        else:
          # The user existed.
          raise Exception(error.EMAIL_ALREADY_REGISTERED)
      user_id = user['_id']
      self.__generate_identity(user, password)
      with elle.log.trace("add user to the mailing list"):
        self.invitation.subscribe(email)
      self._notify_swaggers(
        notifier.NEW_SWAGGER,
        {
          'user_id' : str(user_id),
        },
        user_id = user_id,
      )

      if not user.get('email_confirmed', False):
        self.mailer.send_template(
          to = email,
          template_name = 'confirm-sign-up',
          merge_vars = {
            email: {
              'CONFIRM_KEY': key('/users/%s/confirm-email' % user_id),
              'USER_FULLNAME': fullname,
              'USER_ID': str(user_id),
            }}
        )
      return self.__user_view(self.__user_fill(user))

  def __generate_identity(self, user, password):
    with elle.log.trace('generate identity'):
      if password is None:
        password = ''
      identity, public_key = papier.generate_identity(
        str(user['_id']),
        str(self.user_identifier(user)),
        password,
        conf.INFINIT_AUTHORITY_PATH,
        conf.INFINIT_AUTHORITY_PASSWORD
        )
      update = {
        'identity': identity,
        'public_key': public_key,
      }
      self.database.users.update(
        {
          '_id': user['_id'],
        },
        {
          '$set': update,
        },
      )
      user.update(update)
      return user

  # Deprecated
  @api('/user/confirm_email/<hash>', method = 'POST')
  def _confirm_email(self,
                    hash: str):
    with elle.log.trace('confirm email'):
      try:
        user = self.__user_fetch(
          {'email_confirmation_hash': hash},
          fields = ['email'],
        )
        if user is None:
          raise error.Error(
            error.OPERATION_NOT_PERMITTED,
            'No user could be found',
          )
        elle.log.trace('confirm %s\'s account' % user['email'])
        self.database.users.update(
          {'email_confirmation_hash': hash},
          {
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
      user = self.database.users.find_and_modify(
        query,
        {
          '$unset': {'unconfirmed_email_leeway': True},
          '$set': {'email_confirmed': True}
        },
        fields = ['_id'])
      if user is None:
        self.forbidden({
          'user': user,
          'reason': 'invalid confirmation hash or email',
        })
      return self._web_login(user)

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
      user = self.user_by_id_or_email(user,
                                      fields = [
                                        'email',
                                        'email_confirmed',
                                        'email_confirmation_hash',
                                        'fullname',
                                      ])
      if user is None:
        response(404, {'reason': 'user not found'})
      if user.get('email_confirmed', True):
        response(404, {'reason': 'email is already confirmed'})
      assert user.get('email_confirmation_hash') is not None
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

  ## ----- ##
  ## Ghost ##
  ## ----- ##
  def generate_random_sequence(self, alphabet = code_alphabet, length = code_length):
    """
    Return a pseudo-random string.

    alphabet -- A list of characters.
    length -- The size of the wanted random sequence.
    """
    import random
    random.seed()
    return ''.join(random.choice(alphabet) for _ in range(length))

  def __register_ghost(self,
                       extra_fields,
                       recipient = None):
    assert 'accounts' in extra_fields
    plan = self.database.plans.find_one({'name': 'basic'})
    features = self._roll_features(True)
    quota = {}
    if plan is not None:
      features.update(plan.get('features', {}))
      quota =  plan.get('quota', {})
    ghost_code = self.generate_random_sequence()
    request = {
      'register_status': 'ghost',
      'notifications': [],
      'networks': [],
      'devices': [],
      'swaggers': {},
      'features': features,
      # Ghost code is used for merging mechanism.
      'ghost_code': ghost_code,
      'ghost_code_expiration': self.now + datetime.timedelta(days=14),
      'quota': quota,
      'plan': 'basic',
    }
    if recipient is not None:
      user_id = recipient['_id']
      request.update(extra_fields)
      del request['accounts']
      self.database.users.update( {'_id': user_id},
        { '$set': request})
    else:
      request.update(extra_fields)
      user_id = self._register(**request)
    self.database.users.update(
      {
        "_id": user_id
      },
      {
        '$set': {
          'shorten_ghost_profile_url': self.shorten(
            self.__ghost_profile_url(
              {
                '_id': user_id,
                'ghost_code': ghost_code,
              },
              type = extra_fields['accounts'][0]["type"],
            ))
        }
      })
    return user_id

  def __ghost_profile_url(self, ghost, type):
    """
    Return the url of the user ghost profile on the website.
    We have to specify the full url because this url will be shorten by another
    service.

    ghost -- The ghost.
    """
    ghost_id = ghost.get('_id', ghost.get('id'))
    assert ghost_id is not None
    # Old ghosts don't have any ghost_code.
    code = ghost.get('ghost_code', '')
    url = '/ghost/%s' % str(ghost_id)
    ghost_profile_url = "https://www.infinit.io/" \
                        "invitation/%(ghost_id)s?key=%(key)s&code=%(code)s" \
                        "&utm_source=%(type)s&utm_medium=invitation" % {
                          'ghost_id': str(ghost_id),
                          'key': key(url),
                          'code': code,
                          'type': type
                        }
    return ghost_profile_url

  @api('/ghost/code/<code>', method = 'GET')
  def check_code(self, code):
    """
    Check if a ghost code exists.

    code -- The code.
    """
    if len(code) == 0:
      return self.bad_request({
        'reason': 'code cannot be empty',
      })
    account = self.database.users.find_one({'ghost_code': code})
    if account is None:
      return self.not_found({
        'reason': 'unknown code : %s' % code,
        'code': code,
      })
    return {}

  @api('/ghost/<code>/merge', method = 'POST')
  @require_logged_in_fields(['used_referral_link'])
  def merge_ghost(self,
                  code : str):
    """
    Use code stored in or passed to the client: can be a ghost code or a
    referral code
    Merge a ghost to an existing user account.
    The code is given (via email, sms) to the recipient.

    code -- The code.
    """
    if len(code) >= 10:
      #referral code
      if self.user.get('used_referral_link', None):
        return {} # already used
      self.database.users.update({'_id': self.user['_id']},
                           {'$set': {'used_referral_link': code}})
      inviter = self.database.users.find_one({'referral_code': code},
                                       fields=['_id'])
      if inviter == None:
        return {}
      self.process_referrals(self.user, [inviter['_id']],
                             inviter_bonus = 500e6,
                             invitee_bonus = 250e6,
                             inviter_cap = 16e9,
                             invitee_cap = 16e9)
      return
    # Ghost code
    with elle.log.trace("merge ghost with code %s" % code):
      if len(code) == 0:
        return self.bad_request({
          'reason': 'code cannot be empty',
        })
      user = self.user
      gone = {
        'reason': 'this user is not accessible anymore',
        'code': code,
      }
      if code in user.get('consumed_ghost_codes', []):
        return self.gone(gone)
      # Because we delete 'ghost_code' from the user, we are not be able to
      # return a clean 'gone' http status.
      account = self.database.users.find_one({'ghost_code': code})
      if account is None:
        return self.not_found({
          'reason': 'unknown code : %s' % code,
          'code': code,
        })
      if account['ghost_code_expiration'] < self.now or \
         account['register_status'] != 'ghost':
        return self.gone(gone)
      elle.log.debug('account found: %s' % account)
      self.user_delete(account, merge_with = user)
      return {}

  @api('/ghost/<user_id>', method = 'GET')
  @require_key
  def ghost_profile(self,
                    user_id : bson.ObjectId):
    """
    Return the ghost data.

    user_id -- The ghost id.
    """
    with elle.log.trace("get ghost page %s" % user_id):
      account = self.__user_fetch(
        {'_id': user_id},
        fields = self.__user_view_fields + ['ghost_code_expiration'])
      if account is None:
        return self.not_found({
          'reason': 'User not found',
          'id': str(user_id),
        })
      if account['register_status'] in ['deleted', 'merged', 'ok']:
        return self.gone({
          'reason': 'This user doesn\'t exist anymore',
          'id': str(user_id),
        })
      transactions = list(self.database.transactions.find(
        {
          'involved': account['_id'],
          'status': {'$nin': transaction_status.final + [transaction_status.CREATED] }
        },
        fields = self.__transaction_hash_fields(include_id = True)
      ))
      # Add the key 'id' (== '_id')
      transactions = [dict(i, id = i['_id']) for i in transactions]
      return {
        'transactions': transactions
      }

  @api('/users/<user>/accounts')
  @require_logged_in
  def accounts_users_api(self, user):
    fields = ['accounts']
    if isinstance(user, bson.ObjectId):
      user = self._user_by_id(user, fields = fields)
    else:
      user = self.user_by_id_or_email(user, fields = fields)
    if not self.admin and user['_id'] != self.user['_id']:
      self.forbidden({
        'reason': 'not your accounts',
      })
    return self.account_list(user)

  def account_list(self, user):
    return {
      'accounts': user.get('accounts', []),
    }

  # Used by website
  @api('/user/accounts')
  @require_logged_in
  def accounts(self):
    user = self.user
    return {
      'email': user.get('email'),
      'accounts': user['accounts'],
    }

  @api('/user/accounts/<email>', method = 'PUT')
  @require_logged_in
  def account_add(self, email: utils.enforce_as_email_address):
    other = self.user_by_email(
      email,
      ensure_existence = False,
      fields = ['register_status'])
    if other is not None and other['register_status'] not in ['contact', 'ghost']:
      self.conflict({
          'reason': 'email already registered',
          'email': email,
        })
    url ='/users/%s/accounts/%s/confirm' % (self.user['_id'], email)
    k = key(url)
    variables = {
      'email': email,
      'user': self.email_user_vars(self.user),
      'url': self.url_absolute(url),
      'key': key(url),
    }
    self.emailer.send_one('account-add-email',
                          email,
                          self.user['fullname'],
                          variables = variables)
    return {}

  @api('/users/<user>/accounts/<name>/confirm', method = 'POST')
  @require_key
  def account_confirm(self, user, name):
    with elle.log.trace('validate email %s for user %s' %
                        (name, user)):
      update = {
        '$addToSet':
        {
          'accounts': collections.OrderedDict((('id', name), ('type', 'email')))
        }
      }
      user = self.user_by_id_or_email(user, fields = ['_id', 'accounts'])
      while True:
        try:
          self.database.users.update(
            {'_id': user['_id']},
            update,
          )
          break
        except pymongo.errors.DuplicateKeyError:
          previous = self.user_by_id_or_email(
            name,
            fields = ['email', 'register_status', 'swaggers'],
            ensure_existence = False)
          if previous is None:
            elle.log.warn('email confirmation duplicate disappeared')
            continue
          # FIXME: don't err if it's ourself
          status = previous['register_status']
          if status in ['ok', 'merged']:
            elle.log.trace(
              'account %s has non-mergeable register status: %s' %
              (name, status))
            self.forbidden({
              'reason': 'email already registered',
              'email': name,
            })
          if status in ['ghost', 'deleted', 'contact']:
            self.user_delete(previous, merge_with = user)
            continue
      return {}

  @api('/user/accounts/<email>', method = 'DELETE')
  @require_logged_in
  def remove_auxiliary_email_address(self,
                                     email: utils.enforce_as_email_address):
    user = self.user
    if user.get('email') == email:
      self.forbidden({
        'reason': 'deleting primary account is forbidden',
      })
    res = self.database.users.update(
      {
        'accounts': {
          '$elemMatch': {'id': email, 'type': 'email'}
        },
        '_id': user['_id'],
        'email': {'$ne': email} # Not the primary email.
      },
      {
        '$pull': {'accounts': collections.OrderedDict((('id', email), ('type', 'email')))}
      })
    if res['n'] == 0:
      return self.not_found({
        'reason': 'no such email address in account: %s' % email,
        'email': email,
      })
    return {}

  @api('/user/accounts/<email>/make_primary', method = 'POST')
  @require_logged_in_fields(['password'])
  def swap_primary_account(self,
                           email: utils.enforce_as_email_address,
                           password):
    user = self.user
    if not any(a['id'] == email for a in user['accounts']):
      self.not_found({
        'reason': 'no such email in account: %s' % email,
        'email': email,
      })
    if user.get('email') == email:
      return {}
    self._change_email(user = user,
                       new_email = email,
                       password = password)
    return {}

  @api('/user/change_email_request', method = 'POST')
  @require_logged_in_fields(['password', 'password_hash'])
  def change_email_request(self,
                           new_email: utils.enforce_as_email_address,
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
        (password, regexp.PasswordValidator),
      ]

      for arg, validator in _validators:
        res = validator(arg)
        if res != 0:
          return self._forbidden_with_error(error.EMAIL_NOT_VALID)

      # Check if the new address is already in use.
      if self.user_by_email(new_email,
                            fields = [],
                            ensure_existence = False) is not None:
        return self._forbidden_with_error(error.EMAIL_ALREADY_REGISTERED)
      if hash_password(password) != user['password'] and utils.password_hash(password) != user['password_hash']:
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
      self.database.users.update(
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
      user = self.__user_fetch(
        {'new_main_email_hash': hash},
        fields = ['new_main_email'],
      )
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
      user = self.__user_fetch(
        {'new_main_email_hash': hash},
        fields = ['new_main_email', 'email'],
      )
      if user is None:
        return self._forbidden_with_error(error.UNKNOWN_USER)
      # Check that the email has not been registered.
      new_email = user['new_main_email']
      if self.user_by_email(new_email,
                            fields = [],
                            ensure_existence = False) is not None:
        return self._forbidden_with_error(error.EMAIL_ALREADY_REGISTERED)
      self._change_email(user, new_email, password)

  def _change_email(self,
                    user,
                    new_email: utils.enforce_as_email_address,
                    password):
    self.remove_session(user)
    # Kick them out of the app.
    self.notifier.notify_some(
      notifier.INVALID_CREDENTIALS,
      recipient_ids = {user['_id']},
      message = {'response_details': 'user email changed'})
    # Cancel transactions as identity will change.
    self.cancel_transactions(user)
    # Handle mailing list subscriptions.
    if 'email' in user:
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
          'password': hash_password(password),
          'identity': identity,
          'public_key': public_key,
        },
        '$unset':
        {
          'new_main_email': '',
          'new_main_email_hash': '',
        },
        '$addToSet': {
          'accounts': {'type': 'email', 'id': new_email}
        },
      })
    return {}

  @api('/user/change_password', method = 'POST')
  @require_logged_in_fields(['password'])
  def change_password(self,
                      old_password,
                      new_password,
                      new_password_hash = None):
    """
    Change the user's password.
    old_password -- the user's old password
    new_password -- the user's new password
    new_password_hash -- the user's new password.
    """
    # Check that the user's passwords are of the correct form.
    _validators = [
      (old_password, regexp.PasswordValidator),
      (new_password, regexp.PasswordValidator),
    ]
    if new_password_hash is not None:
      _validators.append(
        (new_password_hash, regexp.PasswordValidator)
      )
    for arg, validator in _validators:
      res = validator(arg)
      if res != 0:
        return self.fail(res)
    user = self.user
    if 'email' not in user:
      self._forbidden_with_error(error.EMAIL_NOT_CONFIRMED)
    if user['password'] != hash_password(old_password):
      return self.fail(error.PASSWORD_NOT_VALID)
    # Invalidate credentials.
    self.remove_session(user)
    # Kick them out of the app.
    self.notifier.notify_some(
      notifier.INVALID_CREDENTIALS,
      recipient_ids = {self.user['_id']},
      message = {'response_details': 'user password changed'})
    self.logout()
    # Cancel transactions as identity will change.
    self.cancel_transactions(user)

    assert 'email' in user
    with elle.log.trace('generate identity'):
      identity, public_key = papier.generate_identity(
        str(user['_id']),  # Unique ID.
        user['email'],    # Description.
        new_password,     # Password.
        conf.INFINIT_AUTHORITY_PATH,
        conf.INFINIT_AUTHORITY_PASSWORD
      )
    operation = {
      '$set': {
        'password': hash_password(new_password),
        'identity': identity,
        'public_key': public_key
      }
    }
    if new_password_hash is not None:
      operation['$set'].update({
        'password_hash': utils.password_hash(new_password_hash)
      })
    else:
      operation['$unset'] = {
        'password_hash': True
      }
    self.database.users.update(
      {'_id': user['_id']},
      operation
    )
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
  def user_delete_specific(self, user: str):
    self.user_delete(
      self.user_by_id_or_email(user, fields = ['email', 'swaggers']))

  def user_delete(self, user, merge_with = None):
    """The idea is to just keep the user's id and fullname so that transactions
       can still be shown properly to other users. We will leave them in other
       users's swagger lists as we may want to generate the transaction history
       from swaggers rather than transactions.

       We need to:
       - Log the user out and invalidate his session.
       - Delete as much of his information as possible.
       - Remove user as a favourite for other users.
       - Remove user from mailing lists.

       ==============
       = merge_with =
       merge_with argument makes the process transfer elements from the account
       to delete to the 'merge_with' user, including:
       - transactions
       - accounts
       - swaggers (and update swaggers swagger to add merge_with).
       Note that merge with only works with ghost because they have no public
       key, can't be transaction sender and hence the transactions in which
       they are involved can be modified with no risks.

       Considerations:
       - Keep the process as atomic at the DB level as possible.
    """
    if merge_with and user['register_status'] not in ['ghost', 'contact', 'deleted']:
      self.bad_request({
        'reason': 'Only ghost accounts can be merged'
      })

    # Fail early if stripe customer could not be deleted
    try:
      if 'stripe_id' in user:
        cus = stripe.Customer.retrieve(user['stripe_id'],
                api_key = self.stripe_api_key)
        cus.delete()
    # Handle each exception case separately, even though the behaviour is the
    # same for now. Explicit is better than implicit.
    except stripe.error.InvalidRequestError as e:
      # Invalid parameters were supplied to Stripe's API
      elle.log.warn('Unable to delete Stripe customer for user {0}:'
              '{1}'.format(user['email'], e.args))
      return self.unavailable({
        'reason': e.args[0]
      })
    except stripe.error.AuthenticationError as e:
      # Authentication with Stripe's API failed
      # (maybe you changed API keys recently)
      elle.log.warn('Unable to delete Stripe customer for user {0}:'
              '{1}'.format(user['email'], e.args))
      return self.unavailable({
        'reason': e.args[0]
      })
    except stripe.error.APIConnectionError as e:
      # Network communication with Stripe failed
      elle.log.warn('Unable to delete Stripe customer for user {0}:'
              '{1}'.format(user['email'], e.args))
      return self.unavailable({
        'reason': e.args[0]
      })
    except stripe.error.StripeError as e:
      # Display a very generic error to the user, and maybe send
      # yourself an email
      elle.log.warn('Unable to delete Stripe customer for user {0}:'
              '{1}'.format(user['email'], e.args))
      return self.unavailable({
        'reason': e.args[0]
      })

    # Invalidate credentials.
    self.remove_session(user)
    # Kick them out of the app.
    self.notifier.notify_some(
      notifier.INVALID_CREDENTIALS,
      recipient_ids = {user['_id']},
      message = {'response_details': 'user deleted'})
    # If this is somehow a duplicate, do not unregister the user from lists
    if 'email' in user:
      if self.__users_count({'email': user['email']}) == 1:
        self.invitation.unsubscribe(user['email'])
    if merge_with is not None:
      self.change_transactions_recipient(user, merge_with)
      # self.change_links_ownership(user, merge_with)
    else:
      self.cancel_transactions(user)
      self.delete_all_links(user)
      self.remove_devices(user)
    try:
      user.pop('avatar')
      user.pop('small_avatar')
    except:
      elle.log.debug('user has no avatar')
    swaggers = set(map(bson.ObjectId, user['swaggers'].keys()))
    cleared_user = {
      'accounts': [],
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
      'swaggers': {},
    }
    if merge_with is not None:
      ghost_code = None
      if 'ghost_code' in user:
        ghost_code = user['ghost_code']
      cleared_user['register_status'] = 'merged'
      cleared_user['merged_with'] = merge_with['_id']
      deleted_user_swaggers = user.get('swaggers', {})
      deleted_user_contact_of = user.get('contact_of', [])
      # XXX: Obvious race condition here.
      # Accounts fields are unique. If we want to copy user['accounts'] to
      # merge_with, we need to copy and delete them first, and the add them
      # to merge_with.
      # The problem is that during that time lap, someone could send to the
      # user to be deleted email address, creating a new ghost with accounts
      # that can conflit in the database.
      deleted_user_accounts = user.get('accounts', [])
    else:
      cleared_user['register_status'] = 'deleted'
    deleted_user = self.database.users.find_and_modify(
      {'_id': user['_id']},
      {
        '$set': cleared_user,
        '$unset': {
          'avatar': '',
          'small_avatar': '',
          'phone_number': '',
          'ghost_code': '',
          'ghost_code_expiration': '',
          'shorten_ghost_profile_url': '',
        }
      },
      new = True)
    if merge_with is not None:
      # mark invites as completed
      self.database.invitations.update(
        {'recipient': user['_id']},
        {'$set': {'status': 'completed'}},
        multi = True)
      res = self.database.users.find_one({'_id': user['_id']},
        fields = ['referred_by']
        )
      # Apply referrals on the ghost to this user
      self.process_referrals(merge_with, res.get('referred_by', []))
      # Increase swaggers swag for self
      update = {
        '$addToSet': {
          'accounts': {
            '$each': deleted_user_accounts,
          },
        }
      }
      if len(deleted_user_swaggers) > 0:
        update['$inc'] = {
          'swaggers.%s' % \
            id: deleted_user_swaggers[id] for id in deleted_user_swaggers
        }
      if ghost_code is not None:
        update['$addToSet']['consumed_ghost_codes'] = ghost_code
      self.database.users.update({'_id': merge_with['_id']}, update)
      # Increase self swag for swaggers
      for swagger, amount in deleted_user_swaggers.items():
        self.database.users.update(
          {'_id': bson.ObjectId(swagger)},
          {'$inc': {'swaggers.%s' % merge_with['_id']: amount},
          '$unset': {'swaggers.%s' % user['_id']: 1},
          })
        self.notifier.notify_some(
          notifier.NEW_SWAGGER,
          message = {'user_id': swagger},
          recipient_ids = {merge_with['_id']},
        )
      self.change_transactions_recipient(
        current_owner = user, new_owner = merge_with)
      # XXX: We can currently only merge ghosts and ghosts have no links.
      # self.change_links_ownership(user, merge_with)
      new_id = merge_with['_id']
      if len(deleted_user_contact_of):
        self.database.users.update({'_id': {'$in': deleted_user_contact_of}},
          {'$inc': {'swaggers.' + str(new_id): 0}
          }, multi = True)
      for c in deleted_user_contact_of:
        self.notifier.notify_some(
          notifier.NEW_SWAGGER,
          message = {'user_id': str(new_id)},
          recipient_ids = {c},
          )
    self.notifier.notify_some(notifier.DELETED_SWAGGER,
                              recipient_ids = swaggers,
                              message = {'user_id': user['_id']})
    if merge_with:
      self.notifier.notify_some(notifier.NEW_SWAGGER,
                                recipient_ids = swaggers,
                                message = {'user_id': deleted_user['merged_with']})
    self.remove_user_as_favorite_and_notify(user)
    return self.success()

  def remove_user_as_favorite_and_notify(self, user):
    user_id = user['_id']
    recipient_ids = [
      str(u['_id']) for u in
      self.__users_fetch({'favorites': user_id}, fields = ['_id'])
    ]
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

  ## -------------- ##
  ## Search helpers ##
  ## -------------- ##

  def __ensure_user_existence(self, user):
    """Raise if the given user is not valid.

    user -- the user to validate.
    """
    if user is None:
      raise error.Error(error.UNKNOWN_USER)

  def user_by_id_query(self, id):
    assert isinstance(id, bson.ObjectId)
    return id

  def _user_by_id(self, _id, fields, ensure_existence = True):
    """Get a user using by id.

    _id -- the _id of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    assert isinstance(_id, bson.ObjectId)
    user = self.__user_fetch(self.user_by_id_query(_id),
                             fields = fields)
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_public_key(self, key, ensure_existence = True):
    """Get a user from is public_key.

    public_key -- the public_key of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    user = self.__user_fetch({'public_key': key})
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_email_query(self,
                          email: utils.enforce_as_email_address):
    return {
      'accounts': {
        '$elemMatch': {'id': email, 'type': 'email'}
      }
    }
  def user_by_email(self,
                    email,
                    fields = None,
                    ensure_existence = True):
    """Get a user with given email.

    email -- the email of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    if fields is None:
      fields = self.__user_view_fields
    user = self.__user_fetch(
      self.user_by_email_query(email),
      fields = fields,
    )
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_facebook_id_query(self, facebook_id):
    return {
      'accounts': {
        '$elemMatch': {'id': facebook_id, 'type': 'facebook'}
      }
    }

  def user_by_facebook_id(self,
                          facebook_id,
                          fields = None,
                          ensure_existence = True):
    """Get a user with given phone number.

    facebook_id -- the facebook id.
    ensure_existence -- if set, raise if user is invald.
    """
    if fields is None:
      fields = self.__user_view_fields
    user = self.__user_fetch(
      self.user_by_facebook_id_query(facebook_id),
      fields = fields,
    )
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_phone_number_query(self, phone_number):
    return {
      'accounts': {
        '$elemMatch': {'id': phone_number, 'type': 'phone'}
      }
    }

  def user_by_phone_number(self,
                           phone_number,
                           fields = None,
                           ensure_existence = True):
    """Get a user with given phone number.

    phone_number -- the email of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    if fields is None:
      fields = self.__user_view_fields
    user = self.__user_fetch(
      self.user_by_phone_number_query(phone_number),
      fields = fields,
    )
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user


  def user_by_email_password(self,
                             email,
                             password,
                             password_hash,
                             fields,
                             ensure_existence = True):
    """Get a user from his email.

    email -- The email of the user.
    password -- The password for that account.
    ensure_existence -- if set, raise if user is invald.
    """
    if password is None:
      raise error.Error(error.EMAIL_PASSWORD_DONT_MATCH)
    if password_hash is not None:
      user = self.__user_fetch(
        {
          'email': email,
          'password_hash': utils.password_hash(password_hash),
        },
        fields = fields)
      if user is not None:
        return user
      user = self.__user_fetch_and_modify(
        {
          'email': email,
          'password': hash_password(password),
        },
        {
          '$set':
          {
            'password_hash': utils.password_hash(password_hash),
          },
        },
        new = True,
        fields = fields)
    else:
      user = self.__user_fetch(
        {
          'email': email,
          'password': hash_password(password),
        },
        fields = fields)
    if user is None and ensure_existence:
      raise error.Error(error.EMAIL_PASSWORD_DONT_MATCH)
    return user

  def user_by_handle(self, handle, fields, ensure_existence = True):
    """Get a user from is handle.

    handle -- the handle of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    user = self.__users_fetch(
      {'lw_handle': handle.lower()},
      fields = fields)
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_id_or_email(self, id_or_email, fields,
                          ensure_existence = False):
    if not isinstance(id_or_email, bson.ObjectId) and '@' in id_or_email:
      id_or_email = id_or_email.lower()
      return self.user_by_email(id_or_email,
                                fields = fields,
                                ensure_existence = ensure_existence)
    else:
      try:
        id = bson.ObjectId(id_or_email)
        return self._user_by_id(id,
                                fields = fields,
                                ensure_existence = ensure_existence)
      except bson.errors.InvalidId:
        self.bad_request('invalid user id: %r' % id_or_email)

  def user_by_id_or_email_query(self, id_or_email):
    if not isinstance(id_or_email, bson.ObjectId) and '@' in id_or_email:
      id_or_email = id_or_email.lower()
      return self.user_by_email_query(id_or_email)
    else:
      try:
        id = bson.ObjectId(id_or_email)
        return self.user_by_id_equery(id)
      except bson.errors.InvalidId:
        self.bad_request({
          'reason': 'invalid user id: %r' % id_or_email,
          'id': id_or_email,
        })

  ## ------ ##
  ## Search ##
  ## ------ ##

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
      # FIXME: workaround mongo 2.6 which requires a project after a
      # match with an or.
      fields = {f.split('.')[0]: '$%s' % f.split('.')[0]
                for f in self.__user_view_fields}
      fields['swaggers'] = '$swaggers'
      pipeline.append({
        '$project': fields,
      })
      # /FIXME
      if self.logged_in:
        pipeline.append({
          '$sort': {'swaggers.%s' % str(self.user['_id']) : -1}
        })
      pipeline.append({'$skip': skip})
      pipeline.append({'$limit': limit})
      users = self.database.users.aggregate(pipeline)
      for user in users['result']:
        del user['swaggers']
        self.__user_fill(user)
      return {'users': [self.__user_view(u) for u in users['result']]}

  def __users_by_emails_search(self, emails, limit, offset):
    """Search users for a list of emails.

    emails -- list of emails to search with.
    limit -- the maximum number of results.
    offset -- number to skip in search.
    """
    with elle.log.trace("%s: search %s emails (limit: %s, offset: %s)" %
                        (self.user['_id'], len(emails), limit, offset)):
      fields = self.__user_view_fields
      fields.append('email')
      users = self.__users_fetch(
        {
          'accounts.id': {'$in': emails},
          'register_status': 'ok',
        },
        fields = fields,
        limit = limit,
        skip = offset,
      )
      return {'users': [self.__user_view(u) for u in users]}

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

  def user_from_identifier(self,
                           identifier,
                           country_code = None,
                           account_type = None,
                           fields = None):

    identifier = utils.identifier(identifier)
    args = {
      'fields': fields,
      'ensure_existence': False,
    }
    def check_type(account_type, expected):
      return account_type == expected or account_type is None
    user = None
    if isinstance(identifier, bson.ObjectId):
      if check_type(account_type, 'ObjectId'):
        user = self._user_by_id(identifier, **args)
    elif '@' in identifier:
        if check_type(account_type, 'email'):
          user = self.user_by_email(identifier, **args)
    else:
      if account_type == 'facebook':
        user = self.user_by_facebook_id(identifier,
                                        **args)
      elif check_type(account_type, 'phone'):
        if country_code is None and self.current_device is not None:
          country_code = self.current_device.get('country_code', None)
        phone_number = clean_up_phone_number(
          identifier, country_code)
        if phone_number:
          user = self.user_by_phone_number(phone_number, **args)
    return user

  @api('/users/<recipient_identifier>')
  def view_user(self,
                recipient_identifier: utils.identifier,
                country_code = None,
                account_type = None):
    """
    Get user's public information by identifier.
    recipient_identifier -- Something to identify the user (email, user_id or phone number).
    """
    user = self.user_from_identifier(
      identifier = recipient_identifier,
      country_code = country_code,
      account_type = account_type,
      fields = self.__user_view_fields)
    if user is None:
      self.not_found({
        'reason': 'user %s not found' % recipient_identifier,
        'id': recipient_identifier,
      })
    else:
      return self.__user_view(user)

  @api('/users/from_handle/<handle>')
  @require_logged_in
  def view_from_handle(self, handle):
    """
    Get user information from handle
    """
    with elle.log.trace("%s: search user from handle %s" % (self, handle)):
      user = self.user_by_handle(handle,
                                 fields = self.__user_view_fields,
                                 ensure_existence = False)
      if user is None:
        return self.not_found()
      else:
        return self.__user_view(user)

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
          new = True,
          fields = ['swaggers'])
        if res['swaggers'][str(peer)] == 1: # New swagger.
          self.notifier.notify_some(
            notifier.NEW_SWAGGER,
            message = {'user_id': res['_id']},
            recipient_ids = {peer},
          )

  def _swaggers(self):
    user = self.user
    swaggers = user['swaggers']
    users = self.__users_fetch(
      {
        '_id':
        {
          '$in': list(map(bson.ObjectId, swaggers.keys()))
        }
      },
      fields = self.__user_view_fields)
    return sorted(map(self.__user_view, users),
                  key = lambda u: swaggers[str(u['id'])])

  # Backward up to 0.9.27
  @api('/user/full_swaggers')
  @require_logged_in
  def full_swaggers(self):
    return {'swaggers': self._swaggers()}

  @api('/user/swaggers')
  @require_logged_in
  def swaggers(self):
    return {'swaggers': self._swaggers()}

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
    assert isinstance(user_id, bson.ObjectId)
    # FIXME: surely that user is already fetched. It's probably self
    # anyway ...
    user = self._user_by_id(user_id, fields = ['swaggers'])
    swaggers = set(map(bson.ObjectId, user['swaggers'].keys()))
    d = {"user_id" : user_id}
    d.update(data)
    if swaggers:
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
    other = self.__user_fetch({'lw_handle': lw_handle}, fields = [])
    if other is not None and other['_id'] != user['_id']:
      return self.fail(
        error.HANDLE_ALREADY_REGISTERED,
        field = 'handle',
        )
    update = {
      '$set': {'handle': handle, 'lw_handle': lw_handle, 'fullname': fullname}
    }
    self.database.users.update(
      {'_id': user['_id']},
      update)
    return self.success()

  @api('/user/invited')
  @require_logged_in
  def invited(self):
    """Return the list of users invited.
    """
    user = self.user
    source = user.get('email', self.user_identifier(user))
    invitees = self.database.invitations.find(
      {
        'source': source,
      },
      fields = {'email': True, '_id': False}
    )
    return self.success({
      'user': list(map(lambda u: u['email'], invitees)),
    })

  @api('/user/self')
  @require_logged_in_fields(['identity'])
  def user_self(self):
    """Return self data."""
    return self.__user_self(self.user)

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
                 no_place_holder: bool = False,
                 ghost_code: str = ''):
    if len(ghost_code):
      # Link from a mail, notify opened
      self.database.invitations.update(
        {'ghost_code': ghost_code, 'status': 'pending'},
        {'$set': {'status': 'opened'}},
        multi = True
        )
      # send metric
      if self.metrics is not None:
        self.metrics.send(
          [{
            'event': 'invite/opened',
            'ghost_code': ghost_code,
            'user': str(id),
            'timestamp': time.time(),
          }],
          collection = 'users')
    user = self._user_by_id(id,
                            ensure_existence = False,
                            fields = ['small_avatar'])
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
      response.content_type = 'image/jpeg'
      return bytes(small_image)
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
    from PIL import Image
    image = Image.open(request.body)
    self._set_avatar(self.user, image)

  def _set_avatar(self, user, image):
    from io import BytesIO
    small_image = self._small_avatar(image)
    out = BytesIO()
    small_out = BytesIO()
    image.save(out, 'PNG')
    small_image.save(small_out, 'PNG')
    out.seek(0)
    small_out.seek(0)
    import bson.binary
    self.database.users.update(
      {'_id': user['_id']},
      {'$set': {
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
                            status,
                            trophonius_id,
                            version = None,
                            os = None):
    """Add or remove the device from user connected devices.

    device_id -- the id of the requested device
    user_id -- the device owner id
    status -- the new device status
    """
    fmt = {
      'action': 'connected' if status else 'disconnected',
      'device': 'device %s' % device_id,
      'user': 'user %s' % user_id,
    }
    with elle.log.trace('%(user)s %(action)s on %(device)s' % fmt):
      assert isinstance(user_id, bson.ObjectId)
      assert isinstance(device_id, uuid.UUID)
      update_action = status and '$addToSet' or '$pull'
      action = {}
      if version is not None:
        version = collections.OrderedDict(sorted(version.items()))
      match = {
        '_id': user_id,
        'devices': {'$elemMatch': {'id': str(device_id)}},
      }
      if status:
        action['$set'] = {
          'connection_time': self.now,
          'devices.$.trophonius': str(trophonius_id),
          'devices.$.online': True,
          'devices.$.version': version,
          'devices.$.os': os,
          'online': True,
        }
      else:
        match['devices']['$elemMatch']['trophonius'] = str(trophonius_id)
        action['$set'] = {
          'disconnection_time': self.now,
          'devices.$.trophonius': None,
          'devices.$.online': False,
        }
      previous = self.database.users.find_and_modify(
        match,
        action,
        fields = ['devices.id', 'devices.trophonius'],
        new = False,
      )
      if status:
        try:
          device = next(d for d in previous['devices']
                        if d['id'] == str(device_id))
          status_changed = device.get('trophonius', None) == None
        except StopIteration:
          elle.log.warn(
            'could not find device %s in user' % device_id)
          status_changed = True
      else:
        status_changed = previous is not None
      if status_changed:
        if not status:
          self.database.users.update(
            {
              '_id': user_id,
              'devices.online': {'$ne': True},
            },
            {
              '$set': {'online': False,}
            },
          )
          self.__disconnect_endpoint(user_id, device_id)
        self._notify_swaggers(
          notifier.USER_STATUS,
          {
            'status': self._is_connected(user_id),
            'device_id': str(device_id),
            'device_status': status,
          },
          user_id = user_id,
        )
      else:
        if status:
          with elle.log.trace('user reconnected'):
            self.__disconnect_endpoint(user_id, device_id)
            # Simulate a disconnection and a reconnection
            connected = self._is_connected(user_id)
            for status in [False, True]:
              self._notify_swaggers(
                notifier.USER_STATUS,
                {
                  'status': connected,
                  'device_id': str(device_id),
                  'device_status': status,
                },
                user_id = user_id,
              )
        else:
          elle.log.trace('drop obsolete trophonius disconnection')

  def __disconnect_endpoint(self, user, device):
    with elle.log.trace('disconnect user %s device %s '
                        'from transactions' % (user, device)):
      transactions = self.find_nodes(user_id = user,
                                     device_id = device)
      for transaction in transactions:
        tid = transaction['_id']
        elle.log.debug('disconnect from transaction %s' % tid)
        self.update_node(transaction_id = tid,
                         user_id = user,
                         device_id = device,
                         node = None)
        # sender = transaction['sender_id']
        # recipient = transaction['recipient_id']
        # peer =
        self.notifier.notify_some(
          notifier.PEER_CONNECTION_UPDATE,
          recipient_ids = {transaction['sender_id'],
                           transaction['recipient_id']},
          message = {
            "transaction_id": str(transaction['_id']),
            "devices": [transaction['sender_device_id'], transaction['recipient_device_id']],
            "status": False
          }
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
        user = self.__user_fetch_and_modify(
          {"email": user['email']},
          {"$set": {'email_hash': hash}},
          new = True,
          fields = ['email_hash'])
    return user['email_hash']

  def __user_by_email_hash(self, hash, avatar = False, identity=False, passport=False):
    """
    Return the user linked the hash.
    """
    with elle.log.debug('get user from email hash'):
      user = self.__user_fetch({'email_hash': hash},
                               fields = ['_id', 'email'])
      if user is None:
        raise error.Error(
          error.UNKNOWN_USER,
        )
      return user

  @api('/user/email_subscriptions', method = 'GET')
  @require_logged_in_fields(['unsubscriptions'])
  def mail_subscriptions(self):
    """
    Return the status of every subscriptions.
    """
    return self.success({
      'subscriptions':
      {
        k:
        {
          'status': not k in self.user.get('unsubscriptions', []),
          'pretty': mail.subscriptions[k]
        }
        for k in mail.subscriptions.keys()
      }
    })

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
      user = self._user_from_session(fields = ['unsubscriptions'])
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
    user = self.user_by_id_or_email(user, fields = ['email'])
    res = self.__modify_subscription(user, name, True)
    self._web_login(user)
    return res

  @api('/users/<user>/email_subscriptions/<name>', method = 'DELETE')
  @require_key
  def mail_unsubscribe_key(self, user, name):
    """
    Unsubscribe from an email set.

    user -- Email or id of the user to unsubscribe.
    name -- Name of the email set.
    """
    user = self.user_by_id_or_email(user, fields = ['email'])
    if user is None:
      self.not_found({
        'reason': 'user %s not found' % user,
        'user': user,
      })
    res = self.__modify_subscription(user, name, False)
    self._web_login(user)
    return res

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
      users = self.__users_fetch(
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

  @api('/user/synchronize')
  @require_logged_in
  def synchronize(self,
                  init : int = 1,
                  history = 20):
    init = bool(init)
    device = self.current_device
    if device is None:
      # Introspect to give us more info
      did = bottle.request.session.get('device')
      raise Exception('device not found from _id %s' %(did))
    user = self.user
    user2 = self.database.users.find_and_modify(
      query = {'devices.id': device['id'], '_id': user['_id']},
      update = {
        '$set': {
          'devices.$.last_sync': {
            'timestamp': time.time(),
            'date': self.now,
          }
        }})
    device = list(filter(lambda x: x['id'] == device['id'], user2['devices']))[0]
    last_sync = device.get('last_sync', {'timestamp': 1, 'date': datetime.datetime.fromtimestamp(1)})
    # If it's the initialization, pull history, if not, only the one modified
    # since last synchronization!
    res = {
      'swaggers': self._swaggers(),
    }
    mtime = {'timestamp': None, 'date': None}
    if not init:
      mtime = last_sync
    res.update(
      self._user_transactions(
        modification_time = mtime['date'],
        limit = history,
      ))
    # Include deleted links only during updates. At start up, ignore them.
    if self.device_mobile:
      res['links'] = []
    else:
      res.update(self.links_list(mtime = mtime['date'],
                                 include_deleted = not init,
                                 include_canceled = not init))
    # XXX.
    # Return the correct list when iOS 0.9.34* is ready. This workarounds the
    # issue of auto receiving on specific iOS device.
    if self.user_version < (0, 9, 34):
      res.update({'devices': [self.device_view(device)]})
    else:
      res.update(self.devices_users_api(user['_id']))
    res.update(self.accounts_users_api(user['_id']))
    return self.success(res)

  def change_plan(self, uid, new_plan):
    return self._change_plan(self._user_by_id(uid, self.__user_self_fields),
                             new_plan)

  def _change_plan(self, user, new_plan_name):
    current_plan = self.database.plans.find_one({'name': user.get('plan', 'basic')})
    new_plan = self.database.plans.find_one({'name': new_plan_name})
    fset = dict()
    funset = dict()
    for (k, v) in new_plan['features'].items():
      fset.update({'features.' + k: v})
    for (k, v) in current_plan['features'].items():
      if k not in new_plan['features']:
        funset.update({'features.'+k: 1})
    qset = dict()
    qunset = dict()
    for (k, v) in new_plan['quota'].items():
      qset.update({'quota.' + k: v})
    for (k, v) in current_plan['quota'].items():
      if k not in new_plan['quota']:
        qunset.update({'quota.'+k: 1})
    fset.update(qset)
    funset.update(qunset)
    fset.update({'plan': new_plan_name})
    update = {'$set': fset}
    if len(funset):
      update.update({'$unset': funset})
    self.database.users.update({'_id': user['_id']}, update)

  def process_referrals(self, new_user, referrals,
                        inviter_bonus = 1e9,
                        inviter_cap = 10e9,
                        invitee_bonus = 500e6,
                        invitee_cap = 10e9):
    """ Called once per new_user upon first login.
        @param new_user User struct for the invitee
        @param referrals List of user ids who invited new_user
    """
    #policy: +1G for referrers up to 10G,  + 500Mo for referred
    #FIXME: send email
    self.database.users.update(
      {
        '_id': {'$in': referrals},
        '$or': [{'plan': 'basic'}, {'plan': {'$exists': False}}],
        'quota.total_link_size': {'$exists': True, '$lt': inviter_cap}
      },
      {
        '$inc': { 'quota.total_link_size': inviter_bonus}
      },
      multi = True
    )
    self.database.users.update(
      {
        '_id': new_user['_id'],
        '$or': [{'plan': 'basic'}, {'plan': {'$exists': False}}],
        'quota.total_link_size': {'$exists': True, '$lt': invitee_cap}
      },
      {
        '$inc': { 'quota.total_link_size': invitee_bonus}
      },
    )
  @api('/users/<user>', method='PUT')
  @require_logged_in
  def user_update(self,
                  user,
                  plan = None,
                  stripe_token = None):
      user = self.user
      customer = self.__fetch_or_create_stripe_customer(user)
      # Subscribing to a paying plan without source raises an error
      try:
      # Update user's payment source, represented by a token
        if stripe_token is not None:
          customer.source = stripe_token
          customer.save()
        if plan is not None:
          # We do not want multiple plans to be active at the same time, so a
          # customer can only have at most one subscription (ideally, exactly one
          # subscription, which would be 'basic' for non paying customers)
          if customer.subscriptions.total_count == 0:
            customer.subscriptions.create(plan=plan)
          else:
            sub = customer.subscriptions.data[0]
            sub.plan = plan
            sub.save()
          if user.get('plan', 'basic') != plan:
            self.database.users.update(
              {'_id': user['_id']},
              {'$set': {'plan': plan}})
            self._change_plan(user, plan)
      except stripe.error.CardError as e:
        elle.log.warn('Stripe error: customer {0} card'
                      'has been declined'.format(user['_id'],
                      e.args[0]))
        return self.bad_request({
                'reason': (e.args),
                'plan': plan
                })
      except stripe.error.InvalidRequestError as e:
        elle.log.warn('Invalid stripe request, cannot update customer'
                      '{0} plan: {1}'.format(user['_id'],
                      e.args[0]))
        return self.bad_request({
                'reason': (e.args),
                'plan': plan
                })
      except stripe.error.AuthentificationError as e:
        elle.log.warn('Stripe auth failure: {1}'.format(e.args[0]))
        return self.bad_request({
                'reason': (e.args),
                'plan': plan
                })
      except stripe.error.APIConnectionError as e:
        elle.log.warn('Connection to Stripe failed: {1}'.format(e.args[0]))
        return self.unavailable()
      return self.success()

  def __fetch_or_create_stripe_customer(self, user):
    if self.stripe_api_key is None:
      return None
    if 'stripe_id' in user:
      return stripe.Customer.retrieve(
        user['stripe_id'],
        expand = ['subscriptions'],
        api_key = self.stripe_api_key,
      )
    else:
      customer = stripe.Customer.create(
        email = user['email'],
        api_key = self.stripe_api_key,
      )
      self.database.users.update(
        {'_id': user['_id']},
        {'$set': {'stripe_id': customer.id}})
      return customer


  @api('/user/contacts', method='PUT')
  @require_logged_in
  def user_update_contacts(self,
                           contacts):
    """ Update with user address book informations.
        {contacts: [contact...]}
        contact: {phones:[phone], emails:[email]}
    """
    user = self.user
    new_swaggers = dict()
    country_code = self.current_device.get('country_code', None)
    # Normalize phone numbers and email addresses.
    for c in contacts:
      c['phones'] = list(map(lambda x: clean_up_phone_number(x, country_code), c.get('phones', [])))
      c['phones'] = list(filter(lambda x: x is not None, c['phones']))
      c['emails'] = list(map(lambda x: utils.enforce_as_email_address(x, False), c.get('emails', [])))
      c['emails'] = list(filter(lambda x: x is not None, c['emails']))
    mails = [val for sublist in contacts for val in sublist.get('emails', [])]
    phones = [val for sublist in contacts for val in sublist.get('phones', [])]
    ids = mails
    existing = self.database.users.find(
      {'accounts.id': {'$in': ids}},
      fields = ['accounts', 'register_status', 'contact_of']
      )
    existing = list(existing)
    ghosts_to_update = []
    for hit in existing:
      strid = str(hit['_id'])
      if strid not in user['swaggers'] and hit['register_status'] == 'ok':
        new_swaggers['swaggers.' + strid] = 0
      if hit['register_status'] in ['contact','ghost'] and user['_id'] not in hit.get('contact_of', []):
        ghosts_to_update.append(hit['_id'])
    # update contact-of for ghosts and contacts
    self.database.users.update(
      {
      '_id': {'$in': ghosts_to_update},
      'register_status': {'$in': ['ghost', 'contact']}
      },
      {'$addToSet': {'contact_of': user['_id']}},
      multi = True
      )
    # update swaggers
    if len(new_swaggers):
      self.database.users.update({'_id': user['_id']}, {'$inc': new_swaggers})
    for s in new_swaggers.keys():
      self.notifier.notify_some(
          notifier.NEW_SWAGGER,
          message = {'user_id': s.split('.')[1]},
          recipient_ids = {user['_id']},
        )
    # filter contacts not present in db
    in_db = list()
    unmatched = list()
    for hit in existing:
      for a in hit['accounts']:
        in_db.append(a['id'])
    for c in contacts:
      if not any([p in in_db for p in c.get('emails',[]) + c.get('phones', [])]):
        unmatched.append(c)
    # create them in db
    insert = list()
    for c in unmatched:
      phones = c.get('phones', [])
      accounts_mails = list(map(lambda x: {'type': 'email', 'id': x}, c.get('emails', [])))
      accounts_phone = list(map(lambda x: {'type': 'phone', 'id': x}, c.get('phones', [])))
      if len(accounts_mails) == 0: # and len(accounts_phone) == 0:
        continue
      contact_data = {
          'register_status': 'contact',
          'accounts': accounts_mails, # + accounts_phone,
          'phone_numbers': phones,
          'notifications': [],
          'networks': [],
          'devices': [],
          'swaggers': {},
          'features': self._roll_features(True),
          'contact_of': [user['_id']]
      }
      insert.append(contact_data)
    if len(insert):
      # We did not weed out duplicates from within the user address book
      try:
        self.database.users.insert(insert, ordered = False, continue_on_error = True)
      except Exception as e:
        elle.log.log('Exception while inserting contacts: %s' % e)
    return self.success()

  def __check_gcs(self):
    if self.gcs is None:
      self.not_implemented({
        'reason': 'GCS support not enabled',
      })

  @api('/user/backgrounds/<name>', method = 'PUT')
  @require_logged_in
  def user_background_put_api(self, name):
    self.__check_gcs()
    self.require_premium(self.user)
    t = bottle.request.headers['Content-Type']
    l = bottle.request.headers['Content-Length']
    if t not in ['image/gif', 'image/jpeg', 'image/png']:
      self.unsupported_media_type({
        'reason': 'invalid image format: %s' % t,
        'mime-type': t,
      })
    url = self.gcs.upload_url(
      'backgrounds', '%s/%s' % (self.user['_id'], name),
      content_type = t,
      content_length = l,
      expiration = datetime.timedelta(minutes = 3),
    )
    bottle.response.status = 307
    bottle.response.headers['Location'] = url

  @api('/user/backgrounds/<name>', method = 'GET')
  @require_logged_in
  def user_background_put_api(self, name):
    self.__check_gcs()
    self.require_premium(self.user)
    url = self.gcs.download_url(
      'backgrounds', '%s/%s' % (self.user['_id'], name),
      expiration = datetime.timedelta(minutes = 3),
    )
    bottle.response.status = 307
    bottle.response.headers['Location'] = url

  @api('/user/backgrounds/<name>', method = 'DELETE')
  @require_logged_in
  def user_background_put_api(self, name):
    self.__check_gcs()
    self.require_premium(self.user)
    self.gcs.delete(
      'backgrounds', '%s/%s' % (self.user['_id'], name))
    self.no_content()

  @api('/user/backgrounds', method = 'GET')
  @require_logged_in
  def user_background_put_api(self):
    self.__check_gcs()
    return {
      'backgrounds': self.gcs.bucket_list('backgrounds',
                                          prefix = self.user['_id']),
    }

  def __user_account_domains_edit(self, name, action):
    domain = {'name': name}
    user = self.database.users.find_and_modify(
      {'_id': self.user['_id']},
      {action: {'account.custom-domains': domain}},
      new = False,
    )
    account = user.get('account')
    domains = \
      account.get('custom-domains') if account is not None else ()
    return next(filter(lambda d: d['name'] == name, domains), None), domain


  @api('/user/account/custom-domains/<name>', method = 'PUT')
  @require_logged_in
  def user_account_domains_post_api(self, name):
    old, new = self.__user_account_domains_edit(name, '$addToSet')
    if old is None:
      bottle.response.status = 201
    return new

  @api('/user/account/custom-domains/<name>', method = 'DELETE')
  @require_logged_in
  def user_account_patch_api(self, name):
    old, new = self.__user_account_domains_edit(name, '$pull')
    if old is None:
      self.not_found({
        'reason': 'custom domain %s not found' % name,
        'custom-domain': name,
      })
    return new

  @api('/user/account', method = 'GET')
  @require_logged_in_fields(['account'])
  def user_account_get_api(self):
    return self.user.get('account', {})

  @api('/user/invite', method = 'PUT')
  @require_logged_in
  def user_invite(self, identifier):
    user = self.user
    recipient, created = self.__recipient_from_identifier(identifier, user, True)
    if recipient['register_status'] in ['merged', 'ok']:
      self.forbidden({
          'reason': 'User already registered.'
      })
    if recipient['register_status'] == 'deleted':
      self.forbidden({
          'reason': 'User was deleted.'
      })
    query = {'$addToSet': {'referred_by':user['_id']}}
    ghost_code = recipient.get('ghost_code', None)
    shorten_ghost_profile_url = recipient.get('shorten_ghost_profile_url', None)
    if not ghost_code:
      ghost_code = self.generate_random_sequence()
      query.update({'$set': {'ghost_code': ghost_code}})
      query['$set']['shorten_ghost_profile_url'] = self.shorten(
            self.__ghost_profile_url(
              {
                '_id': recipient['_id'],
                'ghost_code': ghost_code,
              },
              type = recipient['accounts'][0]["type"],
            ))
    self.database.users.update({'_id': recipient['_id']},
                               query)
    code = uuid.uuid4()
    self.database.invitations.insert({
        'code': code,
        'status': 'pending',
        'recipient': recipient['_id'],
        'sender': user['_id'],
        'ghost_code': ghost_code
    })
    is_an_email = utils.is_an_email_address(identifier)
    if is_an_email:
      # send email
      variables = {
        'sender': self.email_user_vars(user),
        'ghost_code': ghost_code,
      }

      # insert our tracker in avatar url
      variables['sender']['avatar'] += '?ghost_code=%s' % ghost_code
      self.emailer.send_one(
        'Plain',
        recipient_email = identifier,
        sender_email = user['email'],
        sender_name = user['fullname'],
        variables = variables,
        )
    return {
      'identifier': identifier,
      'ghost_code': ghost_code,
      'shorten_ghost_profile_url': shorten_ghost_profile_url
    }

  @api('/user/invite/<code>/opened', method = 'POST')
  def user_invite_opened(self, code):
    self.database.invitations.update(
      {'code': code, 'status': 'pending'},
      {'$set': {'status': 'opened'}}
    )
    return {}

  @api('/user/invites', method = 'GET')
  @require_logged_in
  def user_invites(self):
    res = self.database.invitations.find({'sender': self.user['_id']})
    res = [{'recipient': str(r['recipient']), 'status': r['status']} for r in res]
    return {'results': res}

  @api('/user/accounts_facebook', method = 'PUT')
  @require_logged_in
  def user_accounts_facebook(self,
                             short_lived_access_token = None,
                             long_lived_access_token = None):
    if bool(short_lived_access_token) == bool(long_lived_access_token):
      return self.bad_request({
        'reason': 'you must provide short or long lived token'
      })
    try:
      facebook_user = self.facebook.user(
        short_lived_access_token = short_lived_access_token,
        long_lived_access_token = long_lived_access_token)
      user = self.user_by_facebook_id(facebook_user.facebook_id,
                                      fields = None,
                                      ensure_existence = False)
      current = self.user
      if user is None:
        # Add it to our account
        account = {
          'type': 'facebook',
          'id': facebook_user.facebook_id
        }
        self.database.users.update(
          {'_id': current['_id']},
          {'$push': {'accounts': account}}
        )
        self.notifier.notify_some(
          notifier.MODEL_UPDATE,
          message = {
            'accounts': [account]
          },
          recipient_ids = {current['_id']},
          version = (0, 9, 37))
      elif user['_id'] != current['_id']:
        self._forbidden_with_error("This facebook account belongs to another user")
      f = facebook_user.friends
      friend_ids = [friend['id'] for friend in f['data']]
      ## Add self to their swaggers
      self.database.users.update(
        {
          'accounts.id' : {'$in': friend_ids},
          'swaggers.' + str(current['_id']) : {'$exists': False}
        },
        {
         '$inc': {'swaggers.' + str(current['_id']) : 0}
        },
        multi = True
      )
      ## Add them to self swaggers
      existing = self.database.users.find(
        {'accounts.id' : {'$in': friend_ids}},
        fields = ['_id']
      )
      inc = {}
      for e in existing:
        inc['swaggers.' + str(e['_id'])] = 0
      if inc:
        self.database.users.update(
          { '_id': current['_id']},
          {'$inc': inc}
        )
        # Notify both the current user and existing Facebook user.
        for e in existing:
          # Notify current user.
          self.notifier.notify_some(
            notifier.NEW_SWAGGER,
            message = {'user_id': e['_id']},
            recipient_ids = {current['_id']},
          )
          # Notify other user.
          self.notifier.notify_some(
            notifier.NEW_SWAGGER,
            message = {'user_id': current['_id']},
            recipient_ids = {e['_id']},
          )
    except Response as r:
      raise r
    except Exception as e:
      self._forbidden_with_error(e.args[0])
    except error.Error as e:
      self._forbidden_with_error(e.args[0])
    return {}

  @api('/user/referral_code')
  @require_logged_in_fields(['referral_code'])
  def user_referral_code(self):
    # We distinguish ghost code and referral code by their size
    code = self.user.get('referral_code', None)
    if not code:
      code = self.generate_random_sequence(length=10)
      self.database.users.update({'_id': self.user['_id']},
                                 {'$set': {'referral_code': code}})
    count = self.database.users.find({'used_referral_link': code}).count()
    return {'referral_code': code, 'count': count}
