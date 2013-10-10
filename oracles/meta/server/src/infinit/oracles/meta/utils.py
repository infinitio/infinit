import bottle
import inspect
from . import error
from . import conf
from bson import ObjectId
import pymongo

class api:

  def __init__(self, route = None, method = 'GET'):
    self.__route = route
    self.__method = method

  def __call__(self, method):
    method.__route__ = self.__route
    method.__method__ = self.__method
    return method

def require_logged_in(method):
  def wrapper(self, *a, **ka):
    if self.user is None:
      self.forbiden()
    return method(self, *a, **ka)
  return wrapper

def hash_pasword(password):
  import hashlib
  seasoned = password + conf.SALT
  seasoned = seasoned.encode('utf-8')
  return hashlib.md5(seasoned).hexdigest()

# There is probably a better way.
def stringify_object_ids(obj):
  if isinstance(obj, ObjectId):
    return str(obj)
  if hasattr(obj, '__iter__'):
    if isinstance(obj, list):
      return [stringify_object_ids(sub) for sub in obj]
    elif isinstance(obj, pymongo.cursor.Cursor):
      return [stringify_object_ids(sub) for sub in obj]
    elif isinstance(obj, dict):
      return {key: stringify_object_ids(obj[key]) for key in obj.keys()}
    elif isinstance(obj, set):
      return (stringify_object_ids(sub) for sub in obj)
    elif isinstance(obj, str):
      return obj
    else:
      raise TypeError('unsported type %s' % type(obj))
  return obj
