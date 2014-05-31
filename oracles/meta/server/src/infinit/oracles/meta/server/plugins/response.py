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


class Response(Exception):

  def __init__(self, status = 200, body = None):
    self.__status = status
    self.__body = body

  @property
  def status(self):
    return self.__status

  @property
  def body(self):
    return self.__body


def response(status, body):
  raise Response(status, body)


class Plugin(object):

  '''Bottle plugin to generate throw a response.'''

  name = 'meta.short-circuit'
  api  = 2

  def apply(self, f, route):
    def wrapper(*args, **kwargs):
      try:
        return f(*args, **kwargs)
      except Response as response:
        bottle.response.status = response.status
        return response.body
    return wrapper
