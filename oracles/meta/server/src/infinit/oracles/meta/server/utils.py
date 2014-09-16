import decorator
import pymongo
from infinit.oracles.meta.server import conf

from infinit.oracles.utils import api, json_value, utf8_string

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

def hash_pasword(password):
  import hashlib
  seasoned = password + conf.SALT
  seasoned = seasoned.encode('utf-8')
  return hashlib.md5(seasoned).hexdigest()
