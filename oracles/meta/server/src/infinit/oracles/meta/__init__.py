#!/usr/bin/python3

import bottle
import inspect
import pymongo
import re

from .plugins.jsongo import Plugin as JsongoPlugin
from .plugins.failure import Plugin as FailurePlugin
from .plugins.session import Plugin as SessionPlugin

from . import error

from .utils import api, hash_pasword, stringify_object_ids, require_logged_in

from . import user
from . import notifier

from bson import ObjectId

class Meta(bottle.Bottle, user.Mixin):

  def __init__(self, mongo_host = None, mongo_port = None):
    super().__init__()
    db_args = {}
    if mongo_host is not None:
      db_args['host'] = mongo_host
    if mongo_port is not None:
      db_args['port'] = mongo_port
    self.__database = pymongo.MongoClient(**db_args).meta
    self.catchall = False
    # Plugins.
    self.install(FailurePlugin())
    self.__sessions = SessionPlugin(self.__database, 'sessions')
    self.install(self.__sessions)
    self.install(JsongoPlugin())
    # Configuration.
    self.ignore_trailing_slash = True
    # Routing.
    for name, method in inspect.getmembers(
        Meta,
        lambda m: callable(m) and hasattr(m, '__route__')):
      self.__register(method)
    # Notifier.
    self.notifier = notifier.Notifier(self.__database.users)

  def __register(self, method):
    rule = method.__route__
    # Introspect method.
    spec = inspect.getargspec(method)
    del spec.args[0] # remove self
    import itertools
    defaults = spec.defaults or []
    spec_args = dict((name, default)
                     for name, default
                     in itertools.zip_longest(
                       reversed(spec.args),
                       reversed([True] * len(defaults))))
    for arg in re.findall('<(\\w*)>', rule):
      if arg in spec_args:
        del spec_args[arg]
      elif spec.keywords is None:
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
          if spec.keywords is not None:
            kwargs.update(d)
          else:
            key = iter(d.keys()).__next__()
            bottle.abort(400, 'unexpected JSON keys: %r' % key)
      try:
        explode(bottle.request.json)
      except ValueError:
        bottle.abort(400, 'invalid JSON')
      explode(bottle.request.query)
      for argument, default in arguments.items():
        if not default:
          bottle.abort(400, 'missing argument: %r' % argument)
      return method(self, *args, **kwargs)
    # Add route.
    route = bottle.Route(app = self,
                         rule = rule,
                         method = method.__method__,
                         callback = callback)
    self.add_route(route)


  def fail(self, *args, **kwargs):
    FailurePlugin.fail(*args, **kwargs)

  def success(self, res = {}):
    assert isinstance(res, dict)
    res['success'] = True
    return stringify_object_ids(res)

  @property
  def database(self):
    assert self.__database is not None
    return self.__database

  def abort(self, message):
    bottle.abort(500, message)

  def forbiden(self):
    bottle.abort(403)

  @api('/status')
  def status(self):
    return self.success()

  ## -------- ##
  ## Sessions ##
  ## -------- ##

  @api('/user/login', method = 'POST')
  def login(self, email, device, password):
    email = email.lower()
    user = self.__database.users.find_one({
      'email': email,
      'password': hash_pasword(password),
    })
    if user is None:
      self.fail(error.EMAIL_PASSWORD_DONT_MATCH)
    # Remove potential leaked previous session.
    self.__sessions.remove({'email': email, 'device': device})
    bottle.request.session['device'] = device
    bottle.request.session['email'] = email
    return self.success({
        '_id' : self.user['_id'],
        'fullname': self.user['fullname'],
        'email': self.user['email'],
        'handle': self.user['handle'],
        'identity': self.user['identity'],
      })

  @api('/user/logout', method = 'POST')
  @require_logged_in
  def logout(self):
    if 'email' in bottle.request.session:
      del bottle.request.session['device']
      del bottle.request.session['email']
      return self.success()
    else:
      return self.fail(error.NOT_LOGGED_IN)

  @property
  def user(self):
    email = bottle.request.session.get('email', None)
    if email is not None:
      return self.user_by_email(email)
