import bottle
import datetime
import elle
import inspect
import iso8601
import json
import pymongo
import re

ELLE_LOG_COMPONENT = 'infinit.oracles.utils'

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
