import bottle
import bson
import calendar
import datetime
import uuid

def jsonify(value):
  import collections
  if isinstance(value, (bson.ObjectId, uuid.UUID)):
    return str(value)
  elif isinstance(value, dict):
    return dict((key, jsonify(value)) for key, value in value.items())
  elif isinstance(value, collections.Iterable) \
       and not isinstance(value, str):
    return value.__class__(jsonify(sub) for sub in value)
  elif isinstance(value, datetime.datetime):
    return calendar.timegm(value.timetuple())
  else:
    return value

def jsonify_dict(value):
  if isinstance(value, dict):
    return jsonify(value)
  else:
    return value

class Plugin(object):

  '''Bottle plugin to load the beaker session in the bottle request.'''

  name = 'meta.jsongo'
  api  = 2

  @classmethod
  def fail(self, *args, **kwargs):
    raise Session.__Failure(*args, **kwargs)

  def apply(self, callback, route):
    def wrapper(*args, **kwargs):
      res = callback(*args, **kwargs)
      res = jsonify_dict(res)
      return res
    return wrapper
