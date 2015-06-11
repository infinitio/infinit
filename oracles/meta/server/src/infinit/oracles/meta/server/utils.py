import bottle
import bson
import decorator
import inspect
import iso8601
import phonenumbers
import pymongo

from itertools import chain

from infinit.oracles.meta.server import conf, regexp
from infinit.oracles.utils import api, json_value, utf8_string, key

from .plugins.response import response

def require_logged_in_fields(fields):
  def require_logged_in(method):
    if hasattr(method, '__api__'):
      raise Exception(
        'require_logged_in for %r wraps the API' % method.__name__)
    def wrapper(wrapped, self, *args, **kwargs):
      # Fuck you, just fuck you MongoDB
      self_fields = [
        f for f in self._Mixin__user_self_fields
        if not any(f.startswith(x) for x in fields)
      ]
      user = self._user_from_session(
        fields = self_fields + fields)
      if self.user is None:
        self.forbidden()
      return wrapped(self, *args, **kwargs)
    return decorator.decorator(wrapper, method)
  return require_logged_in

require_logged_in = require_logged_in_fields([])

def require_admin(method):
  if hasattr(method, '__api__'):
    raise Exception(
      'require_admin for %r wraps the API' % method.__name__)
  def wrapper(wrapped, self, *args, **kwargs):
    if not self.admin:
      self.forbidden()
    return wrapped(self, *args, **kwargs)
  return decorator.decorator(wrapper, method)

def require_logged_in_or_admin_fields(fields):
  def require_logged_in_or_admin(method):
    if hasattr(method, '__api__'):
      raise Exception(
        'require_logged_in_or_admin for %r wraps the API' % method.__name__)
    def wrapper(wrapped, self, *args, **kwargs):
      user = self._user_from_session(
        fields = self._Mixin__user_self_fields + fields)
      if self.user is None and not self.admin:
        self.forbidden({
          'reason': 'not logged in',
        })
      return wrapped(self, *args, **kwargs)
    return decorator.decorator(wrapper, method)
  return require_logged_in_or_admin

require_logged_in_or_admin = require_logged_in_or_admin_fields([])

def require_key(method):
  if hasattr(method, '__api__'):
    raise Exception(
      'require_key for %r wraps the API' % method.__name__)
  def require_key(self, *args, **kwargs):
    import sys
    if 'key' not in kwargs:
      if not self.admin:
        # For now, this will be refused by the API as the 'key'
        # parameter is missing.
        self.forbidden()
    else:
      if not self.admin:
        self.check_key(kwargs['key'])
      del kwargs['key']
    return method(self, *args, **kwargs)
  spec = inspect.getfullargspec(method)
  del spec.args[0] # remove self
  if 'key' in spec.args:
    raise Exception(
      'require_key method already has a \'key\' argument')
  spec.args.insert(0, 'key')
  require_key.__fullargspec__ = spec
  return require_key

def hash_password(password):
  if password is None:
    return None
  import hashlib
  seasoned = password + conf.SALT
  seasoned = seasoned.encode('utf-8')
  return hashlib.md5(seasoned).hexdigest()

def password_hash(password):
  import hashlib
  seasoned = password + conf.SALT
  seasoned = seasoned.encode('utf-8')
  return hashlib.sha256(seasoned).hexdigest()

def clean_up_phone_number(phone_number, country_code):
  """Turn an input into a international phone number.
  If the input cannot be interpreted as a phone number, returns None.

  phone_number -- The potential phone number.
  country_code -- The country code associated. If the number already begins
                  with +, country_code is ignored.
  """
  try:
    # Check if the phone number is valid.
    cleaned_phone_number = phonenumbers.parse(phone_number, country_code)
    # Turn it into to its standard international version
    # (e.g. +33 1 92 39 12 48).
    res = phonenumbers.format_number(
      cleaned_phone_number,
      phonenumbers.PhoneNumberFormat.INTERNATIONAL)
    return res
  except phonenumbers.NumberParseException:
    return None

def invalid_email(email, exit_on_failure = True, res = None):
  if exit_on_failure:
    message = {
      'reason': '%s is not a valid email' % email
    }
    if res is not None:
      message.update({'detail': 'got error %s' % (res,)})
    response(400, message)
  else:
    return None

def is_an_email_address(email):
  import re
  return '@' in email and re.match(regexp.Email, email) is not None

def enforce_as_email_address(identifier, exit_on_failure = True):
  if identifier is None:
    return None
  identifier = identifier.strip().lower()
  if is_an_email_address(identifier):
    return identifier
  else:
    return invalid_email(identifier, exit_on_failure)

def identifier(user_identifier, country_code = None):
  if user_identifier is None:
    return None
  if isinstance(user_identifier, bson.ObjectId):
    return user_identifier
  user_identifier = user_identifier.strip()
  try:
    return bson.ObjectId(user_identifier)
  except bson.errors.InvalidId:
    pass
  if '@' in user_identifier:
    return enforce_as_email_address(user_identifier)
  else:
    # Phone or facebook id.
    if country_code:
      phone_number = clean_up_phone_number(user_identifier, country_code)
      if phone_number is not None:
        return phone_number
    return user_identifier

def date_time(str):
  return iso8601.parse_date(str)
