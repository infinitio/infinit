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
               mandrill = None,
  ):
    super().__init__()
    self.__mongo = mongo_connection(
      mongo_host = mongo_host,
      mongo_port = mongo_port,
      mongo_replica_set = mongo_replica_set)
    self.__boulders = set()
    api.register(self)
    if mandrill is not None:
      self.__mandrill = mandrill
    else:
      import mandrill
      self.__mandrill = mandrill.Mandrill(apikey = 'ca159fe5-a0f7-47eb-b9e1-2a8f03b9da86')
    self.__emailer = emailer.MandrillEmailer(self.__mandrill)

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

  @api('/cron')
  def cron(self):
    with elle.log.trace('%s: run jobs' % self):
      response = {}
      for boulder in self.__boulders:
        with elle.log.trace('%s: run %s' % (self, boulder)):
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
