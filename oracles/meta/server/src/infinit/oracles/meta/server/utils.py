import bottle
import decorator
import inspect
import pymongo

from itertools import chain

from infinit.oracles.meta.server import conf
from infinit.oracles.utils import api, json_value, utf8_string, key

def require_logged_in(method):
  if hasattr(method, '__api__'):
    raise Exception(
      'require_logged_in for %r wraps the API' % method.__name__)
  def wrapper(wrapped, self, *args, **kwargs):
    if self.user is None:
      self.forbidden()
    return wrapped(self, *args, **kwargs)
  return decorator.decorator(wrapper, method)

def require_admin(method):
  if hasattr(method, '__api__'):
    raise Exception(
      'require_admin for %r wraps the API' % method.__name__)
  def wrapper(wrapped, self, *args, **kwargs):
    if not self.admin:
      self.forbidden()
    return wrapped(self, *args, **kwargs)
  return decorator.decorator(wrapper, method)

def require_logged_in_or_admin(method):
  if hasattr(method, '__api__'):
    raise Exception(
      'require_logged_in_or_admin for %r wraps the API' % method.__name__)
  def wrapper(wrapped, self, *args, **kwargs):
    if not self.logged_in and not self.admin:
      self.forbidden()
    return wrapped(self, *args, **kwargs)
  return decorator.decorator(wrapper, method)

def require_key(method):
  if hasattr(method, '__api__'):
    raise Exception(
      'require_key for %r wraps the API' % method.__name__)
  def require_key(self, *args, **kwargs):
    import sys
    if 'key' not in kwargs:
      # For now, this will be refused by the API as the 'key'
      # parameter is missing.
      self.forbidden()
    if kwargs['key'] != key(bottle.request.path):
      self.forbidden()
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

def hash_pasword(password):
  import hashlib
  seasoned = password + conf.SALT
  seasoned = seasoned.encode('utf-8')
  return hashlib.md5(seasoned).hexdigest()
