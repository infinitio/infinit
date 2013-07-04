import errno
import json
import os
import os.path
import socket
import subprocess
import tempfile
import time

root_dir = os.path.realpath(os.path.dirname(__file__))

class Trophonius:

    # XXX Trophonius timeout is linked to plasma/trophonius/Client ping interval.
    # As such, this value must always be greater than the client ping interval.
    def __init__(self,
                 host = "0.0.0.0",
                 port = 0,
                 control_port = 0,
                 meta_host = 'localhost',
                 meta_port = 8080,
                 timeout= 60):
        self.host = host
        self.port = port
        self.control_port = control_port
        self.meta_url = "http://%s:%s" % (meta_host, meta_port)
        self.instance = None
        self.__directory = tempfile.TemporaryDirectory()
        self.timeout = timeout

    def __port_path(self, path):
        return os.path.join(self.__directory.name, path)

    def __read_port(self, path):
        with open(self.__port_path(path), 'r') as f:
            return(int(f.readline()))

    def __enter__(self):
        self.__directory.__enter__()
        path = os.path.join(
          root_dir, '..', '..', '..', 'bin', 'trophonius-server')
        self.instance = subprocess.Popen(
            [path,
             "--port", str(self.port),
             "--control-port", str(self.control_port),
             "--meta-url", self.meta_url,
             "--runtime-dir", self.__directory.name,
             "--timeout", str(self.timeout),
            ],
            stdout = subprocess.PIPE,
            stderr = subprocess.PIPE,
        )
        while True:
            try:
                self.port = self.__read_port('trophonius.sock')
                self.control_port = self.__read_port('trophonius.csock')
                break
            except OSError as e:
                if e.errno is not errno.ENOENT:
                    raise
# Changed in version 3.3: IOError used to be raised, it is now an alias of
# OSError. In python 3.3 FileExistsError is now raised if the file opened
# in exclusive creation mode ('x') already exists.
            except IOError as e:
                    pass
            time.sleep(1)
        return self

    def __exit__(self, exception, exception_type, backtrace):
        assert self.instance is not None
        self.instance.terminate()
        while os.path.exists(self.__port_path('trophonius.sock')) and\
              os.path.exists(self.__port_path('trophonius.csock')):
            time.sleep(1)
        self.__directory.__exit__(exception,
                                  exception_type,
                                  backtrace)

    @property
    def stdout(self):
      return self.instance.stdout.read().decode('utf-8')

    @property
    def stderr(self):
      return self.instance.stderr.read().decode('utf-8')


class Client:

    def __init__(self, addr, user_id, device_id, token):
        self.connection = socket.create_connection(addr)
        self.socket = self.connection.makefile()
        self.device_id = device_id
        self.user_id = user_id
        self.sendline({'token': token, "device_id": device_id, "user_id": user_id})

    def sendline(self, data):
        message = json.dumps(data)
        self.connection.sendall(bytes(message + "\n", encoding="utf-8"))

    def readline(self):
        return self.socket.readline()


class Admin(Client):

    def __init__(self, addr):
        self.connection = socket.create_connection(addr)
        self.socket = self.connection.makefile()

    def notify(self, to, msg):
        data = {'to': to}
        data.update(msg)
        self.sendline(data)


class FakeMeta:

    def __init__(self):
        self.instance = None

    def __enter__(self):
        with tempfile.NamedTemporaryFile() as f:
            self.instance = subprocess.Popen(
                [
                    os.path.join(root_dir, 'fake_meta.py'),
                ],
                stdout = subprocess.PIPE,
                stderr = subprocess.PIPE,
            )
            self.host = '0.0.0.0'
            self.port = int(self.instance.stdout.readline())
            self.url = "http://{}:{}/".format(self.host, self.port)
        return self

    def __exit__(self, exception, exception_type, backtrace):
        assert self.instance is not None
        self.instance.terminate()

    @property
    def stdout(self):
      return self.instance.stdout.read().decode('utf-8')

    @property
    def stderr(self):
      return self.instance.stderr.read().decode('utf-8')
