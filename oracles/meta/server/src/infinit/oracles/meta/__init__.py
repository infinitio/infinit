from .utils import expect_json

import bottle
import pymongo

class Meta(bottle.Bottle):


  def __init__(self, mongo_host = None, mongo_port = None):
    super().__init__()
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

  def error(self, error_code, msg = None, **kw):
    assert isinstance(error_code, tuple)
    assert len(error_code) == 2
    if msg is None:
      msg = error_code[1]
    res = {
      'success': False,
      'error_code': error_code[0],
      'error_details': msg,
      }
    res.update(kw)

  ## -------- ##
  ## Sessions ##
  ## -------- ##

  @expect_json(explode_keys = ['email', 'password'])
  def authenticate(self, email, password, optionals):
    user = self.__database.users.find_one({"email": email, "password": password})
    if user is None:
      return self.error(error_code = (30, ""), msg = "lol")
    # Store session.
    return user

  @property
  def user(self):
    # Get user form session.
    return None
