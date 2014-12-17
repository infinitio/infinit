import bottle
import datetime
import elle

from infinit.oracles.utils import api, mongo_connection
from . import emailer
from . import version

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.Sisyphus'

class Sisyphus(bottle.Bottle):

  def __init__(self,
               mongo_host = None,
               mongo_port = None,
               mongo_replica_set = None,
               emailer = None,
  ):
    super().__init__()
    self.__mongo = mongo_connection(
      mongo_host = mongo_host,
      mongo_port = mongo_port,
      mongo_replica_set = mongo_replica_set)
    self.__boulders = {}
    api.register(self)
    self.__emailer = emailer
    def json_bson_dumps(body):
      import bson.json_util
      return bottle.json_dumps(body, default = bson.json_util.default)
    self.install(bottle.JSONPlugin(json_bson_dumps))

  @property
  def mongo(self):
    return self.__mongo

  @property
  def emailer(self):
    return self.__emailer

  @api('/')
  def status(self):
    return {
      'version': version.version,
    }

  @api('/boulders')
  def boulders_status(self):
    with elle.log.trace('%s: get boulders status' % (self)):
      response = {}
      for boulder in self.__boulders:
        response[boulder] = self.boulder_status(boulder)
      return response

  @api('/boulders/<boulder>')
  def boulder_status(self, boulder):
    try:
      boulder = self.__boulders[boulder]
      with elle.log.trace('%s: run %s' % (self, boulder)):
        return boulder.status()
    except KeyError:
      bottle.response.status = 404
      return {
        'reason': 'no such boulder: %s' % boulder,
        'boulder': boulder,
      }

  @api('/boulders', method = 'POST')
  def boulders_run(self):
    with elle.log.trace('%s: run boulders' % (self)):
      response = {}
      for boulder in self.__boulders:
        response[boulder] = self.boulder_run(boulder)
      return response

  @api('/boulders/<boulder>', method = 'POST')
  def boulder_run(self, boulder):
    try:
      boulder = self.__boulders[boulder]
    except KeyError:
      bottle.response.status = 404
      return {
        'reason': 'no such boulder: %s' % boulder,
        'boulder': boulder,
      }
    with elle.log.trace('%s: run %s' % (self, boulder)):
      return boulder.run()

  def __lshift__(self, boulder):
    self.__boulders[str(boulder)] = boulder


class Boulder:

  @property
  def recurrence(self):
    return datetime.timedelta(hours = 1)

  def __init__(self, sisyphus):
    self.sisyphus = sisyphus
    self.sisyphus << self

  def run(self, sisyphus):
    pass

  def __str__(self):
    return self.__class__.__name__
