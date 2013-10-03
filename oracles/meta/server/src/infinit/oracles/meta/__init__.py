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
    self.__database = pymongo.MongoClient(**db_args)
    self.get('/status')(self.status)

  def abort(self, message):
    bottle.abort(500, message)

  def status(self):
    return {}
