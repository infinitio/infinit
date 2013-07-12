#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

"""
Apertus can be instancied with Apertus class.

>>> from apertus import Apertus
>>> app = Apertus(port=8080)
>>> app.run()

"""

import errno
import os
import subprocess
import tempfile
import time


root_dir = os.path.realpath(os.path.dirname(__file__))

class Apertus:
    def __init__(self,
                 port = 0):
        self.port = port
        self.instance = None
        self.__directory = tempfile.TemporaryDirectory()
        self.__port_file = None

    def __read_port_file(self):
        while True:
            time.sleep(1)
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
        self.port = 0
        for line in content:
            if line.startswith("control"):
                name, value = line.split(':')
                self.port = int(value.strip())

    def __enter__(self):
        command = []
        command.append(os.path.join(root_dir, '..', '..', '..',
                                    'bin', 'apertus-server'))
        self.__directory.__enter__()
        self.__port_file = '%s/apertus.sock' % self.__directory.name
        command.append('--port')
        command.append(str(self.port))
        command.append('--runtime-dir')
        command.append(self.__directory.name)
        self.instance = subprocess.Popen(
            command,
            #stdout = subprocess.PIPE,
            #stderr = subprocess.PIPE,
        )
        self.__read_port_file()
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
