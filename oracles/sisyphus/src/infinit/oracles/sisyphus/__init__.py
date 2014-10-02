import bottle
import datetime
import elle

from infinit.oracles.utils import api, mongo_connection
from . import version

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.Sisyphus'


class Sisyphus(bottle.Bottle):

  def __init__(self,
               mongo_host = None,
               mongo_port = None,
               mongo_replica_set = None,
  ):
    import mandrill
    super().__init__()
    self.__mongo = mongo_connection(
      mongo_host = mongo_host,
      mongo_port = mongo_port,
      mongo_replica_set = mongo_replica_set)
    self.__boulders = set()
    api.register(self)
    self.__mandrill = mandrill.Mandrill(apikey = 'ca159fe5-a0f7-47eb-b9e1-2a8f03b9da86')

  @property
  def mongo(self):
    return self.__mongo

  @property
  def mandrill(self):
    return self.__mandrill

  @api('/')
  def status(self):
    return {
      'version': version.version,
    }

  @api('/cron')
  def status(self):
    response = {}
    for boulder in self.__boulders:
      response[str(boulder)] = boulder.run()
    return response

  def __lshift__(self, boulder):
    self.__boulders.add(boulder)


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
