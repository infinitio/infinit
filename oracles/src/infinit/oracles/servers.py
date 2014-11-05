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
import reactor

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
               setup_client = True,
               with_apertus = True):
    self.__force_admin = force_admin
    self.__mongo_dump = mongo_dump
    self.__force_meta_port = force_meta_port
    self.__force_trophonius_port = force_trophonius_port
    self.__setup_client = setup_client
    self.__cleanup_dirs = list()
    self.__states = list()
    self.__with_apertus = with_apertus
    self._trophonius = None
    self._apertus = None
    self._mongo = None
    self._meta = None
    self._use_productionn = os.environ.get('TEST_USE_PRODUCTION', None) is not None

  def __enter__(self):
    if self._use_productionn:
      print('\n\n###### USING PRODUCTION SERVERS ####\n\n')
      self.meta = ('https', 'meta.9.0.api.production.infinit.io', 443)
    else:
      elle.log.trace('starting mongobox')
      self._mongo = mongobox.MongoBox(dump_file = self.__mongo_dump)
      self._mongo.__enter__()
      elle.log.trace('starting meta')
      self._meta = MetaWrapperProcess(self.__force_admin, self._mongo.port, self.__force_meta_port)
      self._meta.start()
      self.trophonius_start()
      if self.__with_apertus:
        elle.log.trace('starting apertus')
        self._apertus = infinit.oracles.apertus.server.Apertus('http', '127.0.0.1', self._meta.port, '127.0.0.1', 0, 0, timedelta(seconds = 10), timedelta(minutes = 5))
        elle.log.trace('ready')
        self.apertus = ('tcp', '127.0.0.1', self._apertus.port_tcp(), self._apertus.port_ssl())
      self.meta = ('http', '127.0.0.1', self._meta.port)
    if self.__setup_client:
      # Some part of the systems use device_id as an uid (trophonius)
      # So force each State to use its own.
      os.environ['INFINIT_FORCE_NEW_DEVICE_ID'] = '1'
      os.environ['INFINIT_NO_DIR_CACHE'] = '1'
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
    for s in self.__states:
      try:
        s.logout()
      except:
        pass
    reactor.sleep(datetime.timedelta(milliseconds = 300))
    for d in self.__cleanup_dirs:
      elle.log.trace('Cleaning up %s' % d)
      shutil.rmtree(d)
    reactor.sleep(datetime.timedelta(milliseconds = 300))
    if self._trophonius is not None:
      self._trophonius.terminate()
      self._trophonius.stop()
    if self._apertus is not None:
      self._apertus.stop()
    reactor.sleep(datetime.timedelta(milliseconds = 300))
    #self._trophonius.wait()
    if self._meta is not None:
      self._meta.stop()
    if self._mongo is not None:
      self._mongo.__exit__(*args, **kwargs)
  def trophonius_start(self):
    elle.log.trace('starting tropho')
    self.trophonius_stop() # there can be only one for now
    tropho_tcp_port = 0
    # Note: we are actually setting the ssl port, which is the one used
    if self.__force_trophonius_port is not None:
      tropho_tcp_port = self.__force_trophonius_port
    self._trophonius = infinit.oracles.trophonius.server.Trophonius(
      tropho_tcp_port, 0, 'http', '127.0.0.1', self._meta.port, 0,
      timedelta(seconds=30), timedelta(seconds = 60), timedelta(seconds=10))
    self.trophonius = ('tcp', '127.0.0.1', self._trophonius.port_tcp(), self._trophonius.port_ssl())
    elle.log.trace('tropho started on %s' % self._trophonius.port_tcp())
  def trophonius_stop(self):
    if self._trophonius is not None:
      elle.log.trace("tropho term")
      self._trophonius.terminate()
      elle.log.trace("tropho stop")
      self._trophonius.stop()
      elle.log.trace("tropho wait")
      self._trophonius.wait()
    self._trophonius = None
  @property
  def mongo(self):
    return self._mongo

  def state(self):
    """ Construct a new client. That will be delogued on teardown.
    """
    import state
    meta_proto, meta_host, meta_port = self.meta
    res = state.State(meta_proto, meta_host, meta_port, self.download_dir)
    self.__states.append(res)
    return res
