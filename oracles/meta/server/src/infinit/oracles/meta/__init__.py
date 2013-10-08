import bottle
import pymongo
import sys

from .plugins.failure import Plugin as FailurePlugin
from .utils import expect_json

class Meta(bottle.Bottle):

  def fail(self, *args, **kwargs):
    FailurePlugin.fail(*args, **kwargs)

  def __init__(self, mongo_host = None, mongo_port = None):
    super().__init__()
    self.install(plugins.failure.Plugin())
    self.ignore_trailing_slash = True
    db_args = {}
    if mongo_host is not None:
      db_args['host'] = mongo_host
    if mongo_port is not None:
      db_args['port'] = mongo_port
    self.__database = pymongo.MongoClient(**db_args).meta
    self.get('/status')(self.status)
    self.post('/authenticate')(self.authenticate)

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

  @expect_json(explode_keys = ['email', 'password'])
  def authenticate(self, email, password, optionals):
    print('LOL')
    user = self.__database.users.find_one({"email": email, "password": password})
    if user is None:
      self.fail(error_code = (30, ""), message = "lol")
    # Store session.
    return user

  @property
  def user(self):
    # Get user form session.
    return None
