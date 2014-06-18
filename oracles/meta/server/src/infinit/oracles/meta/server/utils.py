import bottle
import bson
import datetime
import decorator
import inspect
import iso8601
import json
import uuid

from . import error
from . import conf
import pymongo

ADMIN_TOKEN = "admintoken"

class api:

  functions = []

  def __init__(self, route = None, method = 'GET'):
    self.__route = route
    self.__method = method

  def __call__(self, method):
    import inspect
    spec = inspect.getfullargspec(method)
    def annotation_mapper(self, *args, **kwargs):
      for arg, annotation in spec.annotations.items():
        value = kwargs.get(arg, None)
        if arg is not None and value is not None:
          try:
            if annotation is datetime.datetime:
              if not isinstance(value, datetime.datetime):
                value = iso8601.parse_date(value)
            else:
              value = annotation(value)
            kwargs[arg] = value
          except:
            m = '%r is not a valid %s' % (value, annotation.__name__)
            self.bad_request(m)
      return method(self, *args, **kwargs)
    annotation_mapper.__route__ = self.__route
    annotation_mapper.__method__ = self.__method
    annotation_mapper.__underlying_method__ = method
    annotation_mapper.__api__ = None
    annotation_mapper.__name__ = method.__name__
    api.functions.append(annotation_mapper)
    return annotation_mapper

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

def hash_pasword(password):
  import hashlib
  seasoned = password + conf.SALT
  seasoned = seasoned.encode('utf-8')
  return hashlib.md5(seasoned).hexdigest()

def json_value(s):
  return json.loads(utf8_string(s))

def utf8_string(s):
  return s.encode('latin-1').decode('utf-8')
