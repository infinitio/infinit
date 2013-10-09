import bottle
import pymongo
import sys

from .plugins.jsongo import Plugin as JsongoPlugin
from .plugins.failure import Plugin as FailurePlugin
from .plugins.session import Plugin as SessionPlugin
from .utils import expect_json

class Meta(bottle.Bottle):

  def __init__(self, mongo_host = None, mongo_port = None):
    super().__init__()
    # Database
    db_args = {}
    if mongo_host is not None:
      db_args['host'] = mongo_host
    if mongo_port is not None:
      db_args['port'] = mongo_port
    self.__database = pymongo.MongoClient(**db_args).meta
    # Plugins
    self.install(FailurePlugin())
    self.__sessions = SessionPlugin(self.__database, 'sessions')
    self.install(self.__sessions)
    self.install(JsongoPlugin())
    # Configuration
    self.ignore_trailing_slash = True
    # Routing
    self.get('/status')(self.status)
    self.get('/self')(self.self)
    self.post('/login')(self.login)
    self.post('/logout')(self.logout)

  def fail(self, *args, **kwargs):
    FailurePlugin.fail(*args, **kwargs)

  @property
  def database(self):
    assert self.__database is not None
    return self.__database

  def abort(self, message):
    bottle.abort(500, message)

  def status(self):
    return {}

  ## -------- ##
  ## Sessions ##
  ## -------- ##

  @expect_json(explode_keys = ['email', 'device', 'password'])
  def login(self, email, device, password, optionals):
    user = self.__database.users.find_one({
      'email': email,
      'password': password
    })
    if user is None:
      self.fail(error_code = (30, ''), message = 'lol')
    # Remove potential leaked previous session.
    self.__sessions.remove({'email': email, 'device': device})
    bottle.request.session['device'] = device
    bottle.request.session['email'] = email
    bottle.request.session['user'] = user
    return {'success': True, 'user': user}

  def logout(self):
    if 'user' in bottle.request.session:
      del bottle.request.session['device']
      del bottle.request.session['email']
      del bottle.request.session['user']
      return {'success': True}
    else:
      return {'success': False}

  def self(self):
    user = self.user
    if user is not None:
      return {'success': True, 'user': user}
    else:
      self.fail((30, ''), message = 'not logged in')

  @property
  def user(self):
    return bottle.request.session.get('user', None)
