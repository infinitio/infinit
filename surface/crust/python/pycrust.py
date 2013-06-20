import _crust
import os

class Env():
    @property
    def host(self):
        return os.getenv("INFINIT_REMOTE_HOST")

    @property
    def port(self):
        return int(os.getenv("INFINIT_REMOTE_PORT", '0'))

    @property
    def token(self):
        return os.getenv("INFINIT_REMOTE_TOKEN_PATH")

    @property
    def home(self):
        return os.getenv("INFINIT_HOME")

#
# Id
#
class ID(_crust.ID):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

#
# Network
#
class Network(_crust._Network):
    __env__ = Env()
    descriptor_list = _crust.descriptor_list

    # Forward all the arguments to _Network contructor.
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    # Store the descriptor to the given path.
    def store(self, path, force = False):
        self._store(path, force)

    # Delete the descriptor to the given path.
    def erase(self, path = ""):
        self._erase(path)

    # Install.
    def install(self,
                network_name,
                owner_name,
                home = __env__.home):
        self._install(network_name, owner_name, home)

    # Uninstall.
    def uninstall(self,
                  network_name,
                  owner_name,
                  home = __env__.home):
        self._uninstall(network_name, owner_name, home)

    # Mount the descritor to the given folder.
    def mount(self, path):
        self._mount(path)

    # Unmount the descritor to the given folder.
    def unmount(self, path):
        self._unmount(path)

    # Store the descriptor on the given remote.
    def publish(self,
                network_name,
                host = __env__.host,
                port = __env__.port,
                token = __env__.token):

        self._publish(network_name, host, port, token)

    # Unpublish the descriptor to the network.
    @staticmethod
    def unpublish(identifier,
                  host = __env__.host,
                  port = __env__.port,
                  token = __env__.token):
        _crust._Network_unpublish(identifier, host, port, token)

    # List the local descriptors.
    @staticmethod
    def list(user_name, infinit_home):
        return _crust._Network_list(user_name,
                                    infinit_home)

    @staticmethod
    def fetch(host = __env__.host,
              port = __env__.port,
              token = __env__.token):
        return _crust._Network_fetch(host, port, token)

    @staticmethod
    def lookup(owner_handle,
               network_name,
               host = __env__.host,
               port = __env__.port,
               token = __env__.token):
        return _crust._Network_lookup(owner_handle,
                                      network_name,
                                      host,
                                      port,
                                      token)

#
# User
#
class User(_crust._User):
    __env__ = Env()

    # Forward all the arguments to _User contructor.
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    # Store the identity to the given path.
    def store(self, path, force = False):
        self._store(path, force)

    # Delete the identity to the given path.
    def erase(self, path = ""):
        self._erase(path)

    # Install.
    def install(self,
                user_name,
                home = __env__.home):
        self._install(user_name, home)

    # Uninstall.
    def uninstall(self,
                  user_name,
                  home = __env__.home):
        self._uninstall(user_name, home)
