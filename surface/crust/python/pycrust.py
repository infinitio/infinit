import _crust
from os import getenv
class MetaData():
  @property
  def host(self):
    return getenv("INFINIT_META_HOST")

  @property
  def port(self):
    return int(getenv("INFINIT_META_PORT"))

  @property
  def token(self):
    token_file_path = getenv("INFINIT_TOKEN_FILE")
    if token_file_path:
      with open(token_file_path) as token_file:
        return token_file.readline()
    return ""

metadata = MetaData()

class Network(_crust._Network):
  # Properties
  properties = (
    "identifier",
    "administrator_K",
    "model",
    "root",
    "everybody_identity",
    "history",
    "extent"
  )

  # Forward all the arguments to _Network contructor.
  def __init__(self, *args, **kwargs):
    _crust._Network.__init__(self, *args, **kwargs)
    for prop in properties:
      setattr(self, prop, property(eval("self._" % property)))

  # Store the descriptor to the given path.
  def store(self, path):
    self._store(path)

  # Store the descriptor on the given remote.
  def store(self, host = metadata.host, port = metadata.port, token = metadata.token):
    self._store(host, port, token)

  # Delete the descriptor to the given path.
  def delete(self, path):
    self._delete(path)

  # Mount the descritor to the given folder.
  def mount(self, path):
    self._mount(path)

  # Unmount the descritor to the given folder.
  def unmount(self, path):
    self._mount(path)

  # Publish the descriptor to the network.
  def publish(self,
              host = metadata.host,
              port = metadata.port,
              token = metadata.token):
    self._publish(host, port, token)

  # Unpublish the descriptor to the network.
  def unpublish(self,
                host = metadata.host,
                port = metadata.port,
                token = metadata.token):
    self._unpublish(host, port, token)

  def list(path):
    return _crust.list(path)

  def list(filter_,
           host = metadata.host,
           port = metadata.port,
           token = metadata.token):
    if not isinstance(filter_, _crust.descriptor_list):
      raise "unknow filter type"
    return _crust.list(filter_, host, port, token)
