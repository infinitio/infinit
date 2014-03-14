#!/usr/bin/python3

import infinit.oracles.trophonius.server
import infinit.oracles.meta.server

import bottle
import elle.log

import threading

import datetime

import time

timedelta = datetime.timedelta

class MetaWrapper(threading.Thread):
  def __init__(self, force_admin=False):
    super().__init__()
    self.meta = infinit.oracles.meta.server.Meta(force_admin = force_admin, enable_emails = False);
  def run(self):
    bottle.run(app=self.meta, host='127.0.0.1')
  def port(self):
    return self.meta.port

def oracles(force_admin=False):
  mw = MetaWrapper(force_admin)
  mw.start()
  time.sleep(2)
  meta_port = mw.port()
  trophonius = infinit.oracles.trophonius.server.Trophonius(0,0, 'http', '127.0.0.1', meta_port, 0, timedelta(seconds=3), timedelta(seconds = 5), timedelta(seconds=7))
  print("Trophonius started")
  return((mw, trophonius),
         ('tcp', '127.0.0.1', meta_port),
         ('tcp', '127.0.0.1', trophonius.port_tcp()), ())
