class Plugin(object):

  '''Bottle plugin to intercept Failure exceptions.'''

  name = 'meta.failure'
  api  = 2

  @classmethod
  def fail(self, *args, **kwargs):
    raise Plugin.__Failure(*args, **kwargs)

  class __Failure(Exception):

    def __init__(self, error_code, message = None, **kw):
      assert isinstance(error_code, tuple)
      assert len(error_code) == 2
      if message is None:
        message = error_code[1]
      self.__json = {
        'success': False,
        'error_code': error_code[0],
        'error_details': message,
      }
      self.__json.update(kw)

    @property
    def json(self):
      return self.__json

  def apply(self, callback, route):
    def wrapper(*a, **ka):
      try:
        return callback(*a, **ka)
      except Plugin.__Failure as f:
        return f.json
    return wrapper
