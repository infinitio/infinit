import elle.log
import bottle
import copy

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.plugin.Session'

class Plugin(object):

  '''Bottle plugin to handle sessions.'''

  name = 'meta.session'
  api  = 2
  secret = '2b291dc8ccf504a106feb41517ba2754b2d1ab57'
  key = 'session-id'

  def __init__(self, database, collection):
    self.__database = database
    self.__collection = collection

  def remove(self, query):
    with elle.log.trace("remove session for %s" % query):
      assert len(query)
      if hasattr(bottle.request, 'session'):
        sid = bottle.request.session_id
        if sid is not None:
          query.update({'_id': {'$ne': sid}})
      res = self.__database.sessions.remove(query)
      elle.log.debug("res: %s" % res)

  def apply(self, callback, route):
    def wrapper(*args, **kwargs):
      collection = self.__database[self.__collection]
      sid = bottle.request.get_cookie(Plugin.key,
                                      secret = Plugin.secret)
      bottle.request.session_id = sid
      bottle.request.session = {}
      if sid is not None:
        session = collection.find_one({'_id': sid})
        if session is not None:
          del session['_id']
          bottle.request.session = session
      previous = copy.deepcopy(bottle.request.session)
      res = callback(*args, **kwargs)
      session = bottle.request.session
      if session != previous:
        if sid is None:
          if len(session):
            sid = collection.save(session)
            bottle.response.set_cookie(Plugin.key,
                                       sid,
                                       secret = Plugin.secret,
                                       max_age = 3600 * 24 * 365)
        elif len(session):
          session['_id'] = sid
          sid = collection.save(session)
        else:
          bottle.response.delete_cookie(Plugin.key)
          collection.remove(sid)

      return res
    return wrapper
