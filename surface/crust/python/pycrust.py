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
    def token_path(self):
        return os.getenv("INFINIT_REMOTE_TOKEN_PATH")

    @property
    def home(self):
        return os.getenv("INFINIT_HOME")

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
    @staticmethod
    def erase(path):
        _crust._Network._erase(path)

    # Install.
    def install(self,
                network_name,
                owner_name,
                home = __env__.home):
        self._install(network_name, owner_name, home)

    # Uninstall.
    @staticmethod
    def uninstall(network_name,
                  owner_name,
                  home = __env__.home):
        _crust._Network._uninstall(network_name, owner_name, home)

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
                token_path = __env__.token_path):

        self._publish(network_name, host, port, token_path)

    # Unpublish the descriptor to the network.
    @staticmethod
    def unpublish(identifier,
                  host = __env__.host,
                  port = __env__.port,
                  token_path = __env__.token_path):
        _crust._Network._unpublish(identifier, host, port, token_path)

    # List the local descriptors.
    @staticmethod
    def list(user_name, infinit_home):
        return _Network._list(user_name,
                                    infinit_home)

    @staticmethod
    def fetch(host = __env__.host,
              port = __env__.port,
              token_path = __env__.token_path):
        return _Network._fetch(host, port, token_path)

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

    # Delete the descriptor to the given path.
    @staticmethod
    def erase(path):
        _crust._User._erase(path)

    # Install.
    def install(self,
                user_name,
                home = __env__.home):
        self._install(user_name, home)

    # Uninstall.
    def uninstall(user_name,
                  home = __env__.home):
        _crust._User._uninstall(user_name, home)

    # Sign in
    def signin(self,
               user_name,
               host = __env__.host,
               port = __env__.port):
        self._signin(user_name, host, port)

    # Sign in
    @staticmethod
    def signout(host = __env__.host,
                port = __env__.port,
                token_path = __env__.token_path):
        _crust._User._signout(host, port, token_path)

    # Login.
    def login(self,
              password,
              host = __env__.host,
              port = __env__.port):
        return self._login(password, host, port)

    # Store token.
    @staticmethod
    def store_token(token,
                    user_name,
                    home):
       return _crust._User._store_token(token, user_name, home)
