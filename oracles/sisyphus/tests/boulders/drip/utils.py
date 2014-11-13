#!/usr/bin/env python3

# Meta first, for papier and OpenSSL
import infinit.oracles.meta.server
from infinit.oracles.meta.server.utils import hash_pasword

import bottle
import datetime
import hashlib
import mongobox

import pymongo
import pymongo.collection
import pymongo.database

class MongoExpectation:

  instance = None

  def __init__(self, index_miss = 0, object_miss = 0):
    self.index_miss = index_miss
    self.object_miss = object_miss

  def __enter__(self):
    self.__previous = MongoExpectation.instance
    MongoExpectation.instance = self

  def __exit__(self, *args, **kwargs):
    MongoExpectation.instance = self.__previous

MongoExpectation.instance = MongoExpectation()

class GestapoMongoClient(pymongo.MongoClient):

  def __getattr__(self, name):
    res = super().__getattr__(name)
    if res.__class__ is pymongo.database.Database:
      res.__class__ = GestapoDatabase
    return res

class GestapoDatabase(pymongo.database.Database):

  def __getattr__(self, name):
    res = super().__getattr__(name)
    if res.__class__ is pymongo.collection.Collection \
       and name != '$cmd':
      res.__class__ = GestapoCollection
    return res

class GestapoCollection(pymongo.collection.Collection):

  def find(self, spec = None, fields = None, *args, **kwargs):
    self.__check(spec, fields)
    return super().find(spec = spec, fields = fields, *args, **kwargs)

  def update(self, spec, document, multi = False):
    self.__check(spec)
    return super().update(spec = spec,
                          document = document,
                          multi = multi)

  def __check(self, condition, fields = None):
    explanation = super().find(condition, fields = fields).explain()
    try:
      # print(explanation['cursor'])
      # print('  ', condition)
      # print('  ', fields)
      # print('  ', explanation)
      if explanation['cursor'] == 'BasicCursor':
        raise Exception('table scan on condition: %s' % condition)
      if explanation.get('scanAndOrder', False):
        raise Exception('scan and order on condition: %s' % condition)
      # The buildfarm doesn't have the allPlans key, but I can't tell
      # whether it's never present or it's omitted if there's only one
      # plan.
      if 'allPlans' in explanation:
        nplans = len(list(e for e in explanation['allPlans']
                          if e['cursor'] != 'BasicCursor'))
        if nplans > 1:
          raise Exception('%s viable plans for condition: %s' % \
                          (nplans, condition))
      ns = explanation['nscanned']
      nso = explanation['nscannedObjects']
      n = explanation['n']
      # print(ns, nso, n)
      if ns - nso > MongoExpectation.instance.index_miss:
        raise Exception('too many index scans (%s) for %s' \
                        ' object scans on condition: %s' % \
                        (ns, nso, condition))
      if nso - n > MongoExpectation.instance.object_miss:
        raise Exception('too many object scans (%s) for %s' \
                        ' results on condition: %s' % \
                        (nso, n, condition))
    except Exception as e:
      import sys
      print('fatal error interpreting explanation: %s' % e,
            file = sys.stderr)
      print('explanation: %s' % explanation, file = sys.stderr)
      raise

class Meta(infinit.oracles.meta.server.Meta):

  @property
  def now(self):
    if hasattr(self, '_Meta__now'):
      return self.__now
    else:
      return super().now

  @now.setter
  def now(self, value):
    self.__now = value

  def __enter__(self):
    def run():
      try:
        bottle.run(app = self,
                   quiet = True,
                   server = self.__server)
      except KeyboardInterrupt:
        pass
    import threading
    self.__server = bottle.WSGIRefServer(port = 0)
    self.__thread = threading.Thread(target = run)
    self.__thread.daemon = True
    self.__thread.start()
    while self.__server.port == 0 and self.__thread.is_alive():
      import time
      time.sleep(.1)
    return self


  def __exit__(self, *args, **kwargs):
    pass


class Mandrill:

  def __init__(self):
    self.__emails = []

  @property
  def emails(self):
    res = self.__emails
    self.__emails = []
    return res

  class Messages:

    def __init__(self, mandrill):
      self.__mandrill = mandrill

    def send_template(self,
                      template_name,
                      template_content,
                      message,
                      async):
      assert async
      for to in message['to']:
        email = to['email']
        found = False
        for v in message['merge_vars']:
          if v['rcpt'] == email:
            human = \
              dict((var['name'], var['content']) for var in v['vars'])
            assert human['USER_EMAIL'] == email
            assert 'USER_FULLNAME' in human
            assert 'UNSUB' in human
            self.__mandrill._Mandrill__emails.append((email, human))
            found = True
            break
        if not found:
          raise Exception('missing merge_vars for %s' % email)

  @property
  def messages(self):
    return Mandrill.Messages(self)
