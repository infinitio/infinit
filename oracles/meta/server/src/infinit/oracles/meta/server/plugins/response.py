import bottle
import bson
import calendar
import datetime
import uuid
from infinit.oracles.meta.server.plugins import jsongo

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
        return jsongo.jsonify_dict(response.body)
    return wrapper
