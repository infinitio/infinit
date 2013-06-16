import _crust
import os

class MetaData():
    @property
    def host(self):
        return os.getenv("INFINIT_REMOTE_HOST")

    @property
    def port(self):
        return int(os.getenv("INFINIT_REMOTE_PORT", '0'))

    @property
    def token(self):
        return os.getenv("INFINIT_REMOTE_TOKEN_PATH")

class ID(_crust.ID):
    def __init__(self, *args, **kwargs):
        _crust.ID.__init__(self, *args, **kwargs)

class Network(_crust._Network):
    __hostdata__ = MetaData()
    descriptor_list = _crust.descriptor_list

    # Forward all the arguments to _Network contructor.
    def __init__(self, *args, **kwargs):
        _crust._Network.__init__(self, *args, **kwargs)

    # Store the descriptor to the given path.
    def store(self, path):
        self._store(path)

    # Delete the descriptor to the given path.
    def erase(self, path = ""):
        self._erase(path)

    # Install.
    def install(self, path):
        self._install(path)

    # Uninstall.
    def uninstall(self, path):
        self._uninstall(path)

    # Mount the descritor to the given folder.
    def mount(self, path):
        self._mount(path)

    # Unmount the descritor to the given folder.
    def unmount(self, path):
        self._unmount(path)

    # Store the descriptor on the given remote.
    def publish(self,
                host = __hostdata__.host,
                port = __hostdata__.port,
                token = __hostdata__.token):

        self._publish(host, port, token)

    # Unpublish the descriptor to the network.
    def unpublish(self,
                  host = __hostdata__.host,
                  port = __hostdata__.port,
                  token = __hostdata__.token):
        self._unpublish(host, port, token)

    # List the local descriptors.
    @staticmethod
    def local_list(path):
        return _crust.list(path)

    @staticmethod
    def remote_list(filter_,
                    host = __hostdata__.host,
                    port = __hostdata__.port,
                    token = __hostdata__.token):
        if not isinstance(filter_, _crust.descriptor_list):
            raise "unknown filter type"
        return _crust.list(filter_, host, port, token)
