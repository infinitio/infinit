# -*- encoding: utf-8 -*-

"""
Meta webserver can be instancied with application.Application class.

>>> from meta.application import Application
>>> app = Application(port=8080)
>>> app.run()

"""

import errno
import os
import sys
import subprocess
import shlex
import time
import traceback

root_dir = os.path.realpath(os.path.dirname(__file__))

class Meta:
    def __init__(self,
                 meta_host = None,
                 meta_port = None,
                 port_file = None,
                 spawn_db = False):

        assert(port_file)
        self.port_file = port_file
        self.meta_host = meta_host
        self.meta_port = meta_port
        self.spawn_db = spawn_db
        self.instance = None

    def __parse_line(self, line = None, item = None):
        if line.startswith(item + ':'):
            return line[len(item + ':'):-1]

    def __read_port_file(self):
        while True:
            try:
                time.sleep(1)
                with open(os.path.abspath(self.port_file), 'r') as f:
                    content = f.readlines()
                    break
            except OSError as e:
                if e.errno is not errno.ENOENT:
                    raise
        for line in content:
            if line.startswith('meta_host'):
                self.meta_host = self.__parse_line(line, 'meta_host')
            if line.startswith('meta_port'):
                self.meta_port = self.__parse_line(line, 'meta_port')
            if line.startswith('mongo_host'):
                self.mongo_host = self.__parse_line(line, 'mongo_host')
            if line.startswith('mongo_port'):
                self.mongo_port = self.__parse_line(line, 'mongo_port')
        return


    def __enter__(self):
        command = []
        command.append(os.path.join(root_dir, '..', '..', '..',
                                    'bin', 'meta-server'))

        if self.port_file != None:
            command.append('--port-file')
            command.append(self.port_file)
        if self.meta_host != None:
            command.append('--meta-host')
            command.append(self.meta_host)
        if self.meta_port != None:
            command.append('--meta-port')
            command.append(self.meta_port)

        if self.spawn_db:
          command.append('--spawn-db')


        self.instance = subprocess.Popen(
            command,
            # stdout = subprocess.PIPE,
            # stderr = subprocess.PIPE,
        )
        self.__read_port_file()
        return self

    def __exit__(self, exception, exception_type, backtrace):
        assert self.instance is not None
        os.remove(self.port_file)
        self.instance.terminate()
