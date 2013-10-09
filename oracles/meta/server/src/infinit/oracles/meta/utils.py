import bottle
from . import error
from . import conf
from bson import ObjectId
import pymongo

class expect_json:

  def __init__(self, expect_keys = [], explode_keys = [], has_optionals = False):
    assert isinstance(expect_keys, (list, set))
    assert isinstance(explode_keys, (list, set))
    self.mandatory_fields = set(expect_keys + explode_keys)
    self.explode_keys = explode_keys
    self.has_optionals = has_optionals

  def __call__(self, method, *args, **kwargs):
    def decorator_wrapper(_self, *args, **kwargs):
      try:
        input = bottle.request.json
        if input is None:
          bottle.abort(400, "body is not JSON")
      except ValueError:
        bottle.abort(400, "body can't be decoded as JSON")
      json = dict(input)
      for key in self.mandatory_fields:
        if key not in json.keys():
          bottle.abort(400, "missing key: %r" % key)
      exploded = {key: json[key] for key in self.explode_keys}
      if self.has_optionals:
        # Probably optimisable.
        optionals = {key: json[key] for key in json.keys() - self.explode_keys}
        print(args, optionals, exploded)
        return method(_self, *args, optionals = optionals, **exploded)
      else:
        return method(_self, *args, **exploded)
    return decorator_wrapper

def require_logged_in(method):
  def wrapper(self, *a, **ka):
    if self.user is None:
      self.fail(error.NOT_LOGGED_IN)
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
      raise TypeError("unsported type %s" % type(obj))
  return obj
