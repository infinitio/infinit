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
                 apertus_host = None,
                 apertus_port = None,
                 spawn_db = False):
        self.meta_host = meta_host
        self.meta_port = meta_port
        self.apertus_host = apertus_host
        self.apertus_port = apertus_port
        self.spawn_db = spawn_db
        self.trophonius_control_port = trophonius_control_port
        self.instance = None
        self.__directory = tempfile.TemporaryDirectory()
        self.__port_file = None
        self.stdout = None
        self.stderr = None
        self.__stdout = tempfile.NamedTemporaryFile()
        self.__stderr = tempfile.NamedTemporaryFile()

    def __parse_line(self, line = None, item = None):
        if line.startswith(item + ':'):
            return line[len(item + ':'):-1]

    def __read_port_file(self):
        while True:
            self.instance.poll()
            if self.instance.returncode is not None:
                raise Exception("meta terminated with status: {}".format(
                        self.instance.returncode))
            try:
                with open(os.path.abspath(self.__port_file), 'r') as f:
                    content = f.readlines()
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
        for line in content:
            if line.startswith('meta_port'):
                self.meta_port = self.__parse_line(line, 'meta_port')


    def __enter__(self):
        command = []
        command.append(os.path.join(root_dir, '..', '..', '..',
                                    'bin', 'meta-server'))
        self.__directory.__enter__()
        self.__stdout.__enter__()
        self.__stderr.__enter__()
        self.__port_file = '%s/port' % self.__directory.name
        command.append('--port-file')
        command.append(self.__port_file)
        command.append('--meta-host')
        command.append(self.meta_host)
        command.append('--meta-port')
        command.append(str(self.meta_port))
        command.append('--apertus-port')
        command.append(str(self.apertus_port))
        command.append('--apertus-host')
        command.append(str(self.apertus_host))
        if self.trophonius_control_port is not None:
            command.append('--trophonius-control-port')
            command.append(str(self.trophonius_control_port))
        if self.spawn_db:
          command.append('--spawn-db')
        self.instance = subprocess.Popen(
            command,
            stdout = self.__stdout,
            stderr = self.__stderr,
        )
        self.__read_port_file()
        self.url = 'http://%s:%s' % (self.meta_host, self.meta_port)
        return self

    def __exit__(self, *args):
        assert self.instance is not None
        import signal
        self.instance.send_signal(signal.SIGINT)
        self.__stderr.flush()
        with open(self.__stderr.name, 'rb') as f:
            self.stderr = f.read().decode('utf-8')
        self.__stderr.__exit__(*args)
        self.__stdout.flush()
        with open(self.__stdout.name, 'rb') as f:
            self.stdout = f.read().decode('utf-8')
        self.__stdout.__exit__(*args)
        self.__directory.__exit__(*args)
