# -*- encoding: utf-8 -*-

"""
Meta webserver can be instancied with application.Application class.

>>> from meta.application import Application
>>> app = Application(port=8080)
>>> app.run()

"""

import errno
import os
import subprocess
import tempfile
import time


root_dir = os.path.realpath(os.path.dirname(__file__))

class Meta:
    def __init__(self,
                 meta_host = '0.0.0.0',
                 meta_port = 0,
                 trophonius_control_port = None,
                 no_apertus = False,
                 spawn_db = False):
        self.meta_host = meta_host
        self.meta_port = meta_port
        self.spawn_db = spawn_db
        self.trophonius_control_port = trophonius_control_port
        self.no_apertus = no_apertus
        self.instance = None
        self.__directory = tempfile.TemporaryDirectory()
        self.__port_file = None

    def __parse_line(self, line = None, item = None):
        if line.startswith(item + ':'):
            return line[len(item + ':'):-1]

    def __read_port_file(self):
        while True:
            try:
                with open(os.path.abspath(self.__port_file), 'r') as f:
                    content = f.readlines()
                    break
                time.sleep(1)
            except OSError as e:
                if e.errno is not errno.ENOENT:
                    raise
# Changed in version 3.3: IOError used to be raised, it is now an alias of
# OSError. In python 3.3 FileExistsError is now raised if the file opened
# in exclusive creation mode ('x') already exists.
            except IOError as e:
                    pass
        for line in content:
            if line.startswith('meta_port'):
                self.meta_port = self.__parse_line(line, 'meta_port')


    def __enter__(self):
        command = []
        command.append(os.path.join(root_dir, '..', '..', '..',
                                    'bin', 'meta-server'))
        self.__directory.__enter__()
        self.__port_file = '%s/port' % self.__directory.name
        command.append('--port-file')
        command.append(self.__port_file)
        command.append('--meta-host')
        command.append(self.meta_host)
        command.append('--meta-port')
        command.append(str(self.meta_port))
        if self.trophonius_control_port is not None:
            command.append('--trophonius-control-port')
            command.append(str(self.trophonius_control_port))
        if self.no_apertus:
            command.append('--no-apertus')
        if self.spawn_db:
          command.append('--spawn-db')
        self.instance = subprocess.Popen(
            command,
            stdout = subprocess.PIPE,
            stderr = subprocess.PIPE,
        )
        self.__read_port_file()
        self.url = 'http://%s:%s' % (self.meta_host, self.meta_port)
        return self

    def __exit__(self, *args):
        assert self.instance is not None
        import signal
        self.instance.send_signal(signal.SIGINT)
        self.__directory.__exit__(*args)

    @property
    def stdout(self):
      return self.instance.stdout.read().decode('utf-8')

    @property
    def stderr(self):
      return self.instance.stderr.read().decode('utf-8')
