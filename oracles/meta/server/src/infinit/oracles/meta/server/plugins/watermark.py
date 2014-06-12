import bottle
import bson
import calendar
import datetime
import decorator
import infinit.oracles.meta.version
import uuid

class Plugin(object):

  '''Bottle plugin to add a meta watermark header.'''

  name = 'meta.watermark'
  api  = 2

  def apply(self, f, route):
    def wrapper(*args, **kwargs):
      import sys
      bottle.response.set_header(
        'X-Fist-Meta-Version',
        str(infinit.oracles.meta.version.version))
      return f(*args, **kwargs)
    return wrapper
