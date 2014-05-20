import infinit.oracles.trophonius.server
import infinit.oracles.apertus.server
import infinit.oracles.meta.server

import bottle
import elle.log

import pymongo
import mongobox

import threading

import datetime

import subprocess
import tempfile
import time
import os
import shutil
import sys

root = os.path.dirname(__file__)

ELLE_LOG_COMPONENT = 'test'

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
  def __init__(self, force_admin = False, mongo_port = None, force_port = None):
    port_file = tempfile.NamedTemporaryFile(delete = False).name
    args = ['%s/../../../../meta/server/bin/meta' % root,
            '--port-file', port_file]
    if force_admin:
      args.append('--force-admin')
    if mongo_port is not None:
      args += ['--mongo-port', str(mongo_port)]
    if force_port is not None:
      args += ['--port', str(force_port)]
    else:
      args += ['--port', '0']
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

  def __init__(self, force_admin = False, mongo_dump = None,
               force_meta_port = None,
               force_trophonius_port = None,
               setup_client = True):
    self.__force_admin = force_admin
    self.__mongo_dump = mongo_dump
    self.__force_meta_port = force_meta_port
    self.__force_trophonius_port = force_trophonius_port
    self.__setup_client = setup_client
    self.__cleanup_dirs = list()

  def __enter__(self):
    elle.log.trace('starting mongobox')
    self._mongo = mongobox.MongoBox(dump_file = self.__mongo_dump)
    self._mongo.__enter__()
    elle.log.trace('starting meta')
    self._meta = MetaWrapperProcess(self.__force_admin, self._mongo.port, self.__force_meta_port)
    self._meta.start()
    elle.log.trace('starting tropho')
    tropho_tcp_port = 0
    # Note: we are actually setting the ssl port, which is the one used
    if self.__force_trophonius_port is not None:
      tropho_tcp_port = self.__force_trophonius_port
    self._trophonius = infinit.oracles.trophonius.server.Trophonius(tropho_tcp_port, 0, 'http', '127.0.0.1', self._meta.port, 0, timedelta(seconds=30), timedelta(seconds = 60), timedelta(seconds=10))
    elle.log.trace('starting apertus')
    self._apertus = infinit.oracles.apertus.server.Apertus('http', '127.0.0.1', self._meta.port, '127.0.0.1', 0, 0, timedelta(seconds = 10), timedelta(minutes = 5))
    elle.log.trace('ready')
    self.meta = ('http', '127.0.0.1', self._meta.port)
    self.trophonius = ('tcp', '127.0.0.1', self._trophonius.port_tcp(), self._trophonius.port_ssl())
    self.apertus = ('tcp', '127.0.0.1', self._apertus.port_tcp(), self._apertus.port_ssl())
    if self.__setup_client:
      # Some part of the systems use device_id as an uid (trophonius)
      # So force each State to use its own.
      os.environ['INFINIT_FORCE_NEW_DEVICE_ID'] = '1'
      # Python will honor environment variables TMPDIR,TEMP,TMP
      if os.environ.get('TEST_INFINIT_HOME', False):
        elle.log.log('Forcing home from environment')
        os.environ['INFINIT_HOME'] = os.environ['TEST_INFINIT_HOME']
      else:
        self.__cleanup_dirs.append(tempfile.mkdtemp('infinit-test'))
        os.environ['INFINIT_HOME'] = self.__cleanup_dirs[-1]
      self.home_dir = os.environ['INFINIT_HOME']
      if os.environ.get('TEST_INFINIT_DOWNLOAD_DIR', False):
        elle.log.log('Forcing download dir from environment')
        os.environ['INFINIT_DOWNLOAD_DIR'] = os.environ['TEST_INFINIT_DOWNLOAD_DIR']
      else:
        self.__cleanup_dirs.append(tempfile.mkdtemp('infinit-test-dl'))
        os.environ['INFINIT_DOWNLOAD_DIR'] = self.__cleanup_dirs[-1]
      self.download_dir = os.environ['INFINIT_DOWNLOAD_DIR']
    return self

  def __exit__(self, *args, **kwargs):
    # FIXME: teardown created State(s)?
    for d in self.__cleanup_dirs:
      elle.log.trace('Cleaning up %s' % d)
      shutil.rmtree(d)
    self._trophonius.terminate()
    self._trophonius.stop()
    self._apertus.stop()
    time.sleep(1)
    #self._trophonius.wait()
    self._meta.stop()
    self._mongo.__exit__(*args, **kwargs)

  @property
  def mongo(self):
    return self._mongo

  def state(self):
    import state
    meta_proto, meta_host, meta_port = self.meta
    tropho_proto, tropho_host, tropho_port_plain, tropho_port_ssl = self.trophonius
    return state.State(meta_proto, meta_host, meta_port, tropho_host, tropho_port_ssl)
