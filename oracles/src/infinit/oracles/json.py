import bson
import datetime
import uuid

def jsonify_value(value):
  if isinstance(value, (bson.ObjectId, uuid.UUID)):
    return str(value)
  elif isinstance(value, datetime.datetime):
    return calendar.timegm(value.timetuple())
  else:
    return value

def jsonify(value):
  import collections
  if isinstance(value, dict):
    return dict((key, jsonify(value)) for key, value in value.items())
  elif isinstance(value, collections.Iterable) \
       and not isinstance(value, str):
    return value.__class__(jsonify(sub) for sub in value)
  else:
    return jsonify_value(value)
