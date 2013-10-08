#!/usr/bin/env python3

import os
import sys

root = os.path.realpath(os.path.dirname(__file__))
sys.path.append(root + '/../src')
sys.path.append(root + '/../../../mongobox')
sys.path.append(root + '/../../../bottle')

import httplib2
import json
import mongobox

import bottle
import infinit.oracles.meta

class Meta:

  def __init__(self):
    self.__mongo = mongobox.MongoBox()
    self.__server = bottle.WSGIRefServer(port = 0)


  def __enter__(self):
    self.__mongo.__enter__()
    def run():
      try:
        app = infinit.oracles.meta.Meta(
          mongo_port = self.__mongo.port)
        app.catchall = False
        bottle.run(app = app,
                   quiet = True,
                   server = self.__server)
      except KeyboardInterrupt:
        pass
    import threading
    self.__thread = threading.Thread(target = run)
    self.__thread.daemon = True
    self.__thread.start()
    while self.__server.port == 0:
      import time
      time.sleep(.1)
    return self

  def __exit__(self, *args, **kwargs):
    self.__mongo.__exit__(*args, **kwargs)

  def post(self, url, body):
    h = httplib2.Http()
    uri = "http://localhost:%s/%s" % (self.__server.port, url)
    headers = {'Content-Type': 'application/json'}
    resp, content = h.request(uri,
                              'POST',
                              body = json.dumps(body),
                              headers=headers)
    status = resp['status']
    if status != '200':
      raise Exception('status %s on /%s with body %s' % (status, url, body))
    if resp['content-type'] == 'application/json':
      return json.loads(content.decode())
    else:
      return content

  def get(self, url):
    h = httplib2.Http()
    uri = "http://localhost:%s/%s" % (self.__server.port, url)
    resp, content = h.request(uri, 'GET')
    status = resp['status']
    if status != '200':
      raise Exception('status %s on /%s with body %s' % (status, url, body))
    return content

  def get_json(self, url):
    return json.loads(self.get(url).decode())

def throws(f):
  try:
    f()
  except:
    pass
  else:
    raise Exception('exception expected')
