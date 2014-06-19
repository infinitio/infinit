import bottle
import infinit.oracles.meta.version

class Plugin(object):

  '''Bottle plugin to send emails on fatal errors.'''

  name = 'meta.fatal_email'
  api  = 2

  def __init__(self, reporter):
    self.__reporter = reporter

  def apply(self, f, route):
    def wrapper(*args, **kwargs):
      try:
        return f(*args, **kwargs)
      except BaseException as e:
        self.__reporter(route = route, exception = e)
        raise
    return wrapper
