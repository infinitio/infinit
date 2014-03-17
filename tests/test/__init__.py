#!/usr/bin/python3

import infinit.oracles.trophonius.server
import infinit.oracles.meta.server

import bottle
import elle.log

import threading

import datetime

import subprocess
import tempfile
import time
import os
import sys

timedelta = datetime.timedelta

class MetaWrapperThread(threading.Thread):
  def __init__(self, force_admin=False):
    super().__init__()
    self.meta = infinit.oracles.meta.server.Meta(force_admin = force_admin, enable_emails = False);
  def run(self):
    bottle.run(app=self.meta, host='127.0.0.1')
  @property
  def port(self):
    return self.meta.port

class MetaWrapperProcess:
  def __init__(self, force_admin = False):
    port_file = tempfile.NamedTemporaryFile(delete = False).name
    os.environ['PYTHONPATH'] = os.environ['PYTHONPATH'] + ':lib/python'
    args = ['../../oracles/meta/server/meta', '--port', '0',
            '--port-file', port_file]
    if force_admin:
      args.append('--force-admin')
    self.process = subprocess.Popen(args, stdout = sys.stdout, stderr = sys.stderr)
    while not os.path.getsize(port_file):
      time.sleep(0.1)
    with open(port_file, 'r') as f:
      self.port = int(f.read())
  def start(self):
    pass
  def stop(self):
    try:
      self.process.terminate()
      self.process.wait(1)
    except:
      pass
    try:
      self.process.kill()
    except:
      pass


class Oracles:

  def __init__(self, force_admin = False):
    self.__force_admin = force_admin

  def __enter__(self):
    self._meta = MetaWrapperProcess(self.__force_admin)
    self._meta.start()
    self._trophonius = infinit.oracles.trophonius.server.Trophonius(0,0, 'http', '127.0.0.1', self._meta.port, 0, timedelta(seconds=3), timedelta(seconds = 5), timedelta(seconds=7))
    self.meta = ('tcp', '127.0.0.1', self._meta.port)
    self.trophonius = ('tcp', '127.0.0.1', self._trophonius.port_tcp())
    return self

  def __exit__(self, *args, **kwargs):
    self._trophonius.terminate()
    self._trophonius.stop()
    time.sleep(1)
    #self._trophonius.wait()
    self._meta.stop()
