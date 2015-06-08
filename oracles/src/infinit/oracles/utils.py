import base64
import bottle
import datetime
import elle
import inspect
import iso8601
import json
import pickle
import pymongo
import re

import Crypto.Cipher.AES as AES
import Crypto.Hash.SHA256 as SHA256
import Crypto.PublicKey.RSA as RSA
import Crypto.Signature.PKCS1_v1_5 as PKCS1_v1_5

ELLE_LOG_COMPONENT = 'infinit.oracles.utils'

class api:

  functions = []

  def __init__(self, route = None, method = 'GET'):
    self.__route = route
    self.__method = method

  def __call__(self, method):
    import inspect
    if hasattr(method, '__fullargspec__'):
      spec = method.__fullargspec__
    else:
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
            # Could be improved (e.g. by catching Response).
            # If the annotation is a type, just 'return' bad_request, otherwise
            # let the exception go throught.
            if type(annotation) is type:
              m = '%r (%s) is not a valid %s' % (
                value, arg, annotation.__name__)
              self.bad_request(m)
            raise
      return method(self, *args, **kwargs)
    annotation_mapper.__route__ = self.__route
    annotation_mapper.__method__ = self.__method
    annotation_mapper.__underlying_method__ = method
    annotation_mapper.__api__ = None
    annotation_mapper.__name__ = method.__name__
    api.functions.append(annotation_mapper)
    return annotation_mapper

  @classmethod
  def register(self, app):
    for method in api.functions:
      self.__register(app, method)

  @classmethod
  def __register(self, app, method):
    rule = method.__route__
    elle.log.debug('%s: register route %s' % (app, rule))
    # Introspect method.
    if hasattr(method.__underlying_method__, '__fullargspec__'):
      spec = method.__underlying_method__.__fullargspec__
    else:
      spec = inspect.getfullargspec(method.__underlying_method__)
      del spec.args[0] # remove self
    import itertools
    defaults = spec.defaults or []
    spec_args = dict((name, default)
                     for name, default
                     in itertools.zip_longest(
                       reversed(spec.args),
                       reversed([True] * len(defaults))))
    for arg in re.findall('<(\\w*)(?::\\w*(?::[^>]*)?)?>', rule):
      if arg in spec_args:
        del spec_args[arg]
      elif spec.varkw is None:
        raise AssertionError(
          'Rule %r yields %r but %r does not accept it' % (rule, arg, method))
    # Callback.
    def callback(*args, **kwargs):
      arguments = dict(spec_args)
      def explode(d):
        if d is None:
          return
        for key in dict(arguments):
          if key in d:
            kwargs[key] = d[key]
            del d[key]
            del arguments[key]
        if len(d) > 0:
          if spec.varkw is not None:
            kwargs.update(d)
          else:
            key = iter(d.keys()).__next__()
            app.bad_request('unexpected JSON keys: %r' % key)
      try:
        explode(bottle.request.json)
      except ValueError:
        app.bad_request('invalid JSON')
      explode(bottle.request.query)
      for argument, default in arguments.items():
        if not default:
          app.bad_request('missing argument: %r' % argument)
      return method(app, *args, **kwargs)
    # Add route.
    route = bottle.Route(app = app,
                         rule = rule,
                         method = method.__method__,
                         callback = callback)
    app.add_route(route)


def json_value(s):
  return json.loads(utf8_string(s))

def utf8_string(s):
  return s.encode('latin-1').decode('utf-8')

def mongo_connection(mongo_host = None,
                     mongo_port = None,
                     mongo_replica_set = None,
):
  if mongo_replica_set is not None:
    with elle.log.log(
        'connect to MongoDB replica set %s' % (mongo_replica_set,)):
      return pymongo.MongoReplicaSetClient(
        ','.join(mongo_replica_set), replicaSet = 'fist-meta')
  else:
    with elle.log.log(
        'connect to MongoDB on %s:%s' % (mongo_host, mongo_port)):
      db_args = {}
      if mongo_host is not None:
        db_args['host'] = mongo_host
      if mongo_port is not None:
        db_args['port'] = mongo_port
      return pymongo.MongoClient(**db_args)

def key(url):
  from hashlib import sha1
  salt = 'aeVi&ap2'
  key = '%s-%s' % (url, salt)
  return sha1(key.encode('latin-1')).hexdigest()

def __sign_cipher():
  return AES.new('8d245bc7b3d6c2f2c13a4a2c675c4e81',
                 AES.MODE_CFB,
                 IV = b'e80919d4715320e1')

def sign(d, expiration, now):
  pickled = pickle.dumps((now + expiration, d))
  crypted = __sign_cipher().encrypt(pickled)
  b64 = base64.urlsafe_b64encode(crypted).decode('latin-1')
  return b64

def check_signature(d, b64, now):
  if b64 is None:
    return False
  try:
    crypted = base64.urlsafe_b64decode(b64.encode('latin-1'))
    pickled = __sign_cipher().decrypt(crypted)
    expiration, sig = pickle.loads(pickled)
    return now <= expiration and sig == d
  except:
    return False
