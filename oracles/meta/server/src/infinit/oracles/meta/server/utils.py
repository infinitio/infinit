import bottle
import decorator
import inspect
import pymongo

from itertools import chain

from infinit.oracles.meta.server import conf
from infinit.oracles.utils import api, json_value, utf8_string, key

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
