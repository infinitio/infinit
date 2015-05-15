import bottle
import io
import os.path

class Fingerprinter(io.RawIOBase):

  def __init__(self, underlying, fingerprint):
    self.__underlying = underlying
    self.__magic = b'INFINIT_FINGERPRINT:'
    self.__fingerprint = self.__magic + fingerprint
    self.__placeholder = self.__magic + b'0123456789ABCDEF'
    assert len(fingerprint) == self.fingerprint_length

  def read(self, size = -1):
    underlying = self.__underlying.read(size)
    return underlying.replace(self.__placeholder, self.__fingerprint)

  fingerprint_length = 16

def add_options(parser):
  parser.add_argument('--directory',
                      metavar = 'DIR',
                      type = str,
                      help = 'directory to serve files from',
                      required = True,
  )

def get_options(options):
  return {
    'directory': options.directory,
  }

class Hephaestus(bottle.Bottle):

  def __init__(self, directory):
    super().__init__(self)
    self.__directory = directory
    if not os.path.isdir(self.__directory):
      raise Exception('%r is not a directory' % self.__directory)
    self.get('<filename:path>')(self.fingerprinter)
    self.get('/')(self.root)

  def fingerprinter(self, filename):
    opener = None
    if 'fingerprint' in bottle.request.query:
      fingerprint = bottle.request.query['fingerprint']
      if len(fingerprint) != Fingerprinter.fingerprint_length:
        bottle.response.status = 400
        return {
          'reason': 'invalid fingerprint',
          'fingerprint': fingerprint,
        }
      fingerprint = fingerprint.encode('latin-1')
      opener = lambda p: Fingerprinter(open(p, 'rb'), fingerprint)
      # uWSGI's sendfile yields the strangest results on those
      # objects, complaining fileno doesn't exist, but importing sys
      # after its invocation fixes it. Disable it to be on the safe
      # side (uwsgi 1.9.17.1-5build5).
      bottle.request.environ.pop('wsgi.file_wrapper', None)
    return bottle.static_file(
      filename,
      download = True,
      root = self.__directory,
      opener = opener)

  def root(self):
    return {}
