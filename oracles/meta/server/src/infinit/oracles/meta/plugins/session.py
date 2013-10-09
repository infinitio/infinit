import bottle

class Plugin(object):

  '''Bottle plugin to load the beaker session in the bottle request.'''

  name = 'meta.session'
  api  = 2

  @classmethod
  def fail(self, *args, **kwargs):
    raise Session.__Failure(*args, **kwargs)

  def apply(self, callback, route):
    def wrapper(*args, **kwargs):
      bottle.request.session = bottle.request.environ['beaker.session']
      return callback(*args, **kwargs)
    return wrapper
