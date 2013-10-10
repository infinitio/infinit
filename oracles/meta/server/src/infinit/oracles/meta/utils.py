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

# class api:

#   def __init__(self, **keys):
#     self.__keys = keys

#   def __call__(self, method):
#     def decorator(_self, *args, **kwargs):
#       try:
#         input = bottle.request.json
#         if input is None:
#           bottle.abort(400, 'body is not JSON')
#       except ValueError:
#         bottle.abort(400, 'body can\'t be decoded as JSON')
#       # Check missing keys that have no default value.
#       specs = inspect.getfullargspec(method)
#       for key in self.__keys:
#         if key not in input:
#           import sys
#           print(specs, file = sys.stderr)
#           if specs.default is None or key not in specs.default:
#             bottle.abort(400, 'missing JSON field: %r' % key)
#       for key, value in input.items():
#         if key not in self.__keys:
#           import sys
#           bottle.abort(400, 'unexpected JSON field: %r' % key)
#         kwargs[key] = value
#       return method(_self, *args, **kwargs)
#     return decorator

def require_logged_in(method):
  def wrapper(self, *a, **ka):
    if self.user is None:
      bottle.abort(403)
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
