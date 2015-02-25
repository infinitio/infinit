#!/usr/bin/env python3

# Meta first, for papier and OpenSSL
import infinit.oracles.meta.server
import elle.log
from infinit.oracles.transaction import statuses

import bottle
import datetime
import hashlib
import mongobox
import uuid

import pymongo
import pymongo.collection
import pymongo.database

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus'

class MongoExpectation:

  instance = None

  def __init__(self, index_miss = 0, object_miss = 0, ignore = False):
    self.index_miss = index_miss
    self.object_miss = object_miss
    self.ignore = ignore

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
    if MongoExpectation.instance.ignore or condition is None:
      return
    explanation = super().find(condition, fields = fields).explain()
    try:
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
    except Exception as e:
      import sys
      print('fatal error interpreting explanation: %s' % e,
            file = sys.stderr)
      print('explanation: %s' % explanation, file = sys.stderr)
      raise
    index_scans = ns - nso > MongoExpectation.instance.index_miss
    object_scans = nso - n > MongoExpectation.instance.object_miss
    if index_scans or object_scans:
      elle.log.err('scanned objects: %s' % list(super().find()))
      elle.log.err('condition: %s' % condition)
      elle.log.err('explanation: %s' % explanation)
      if index_scans:
        raise Exception('too many index scans (%s) for %s' \
                        ' object scans on condition: %s' % \
                        (ns, nso, condition))
      if object_scans:
        raise Exception('too many object scans (%s) for %s' \
                        ' results on condition: %s' % \
                        (nso, n, condition))

def gestapo(client):
  version = client.server_info()['versionArray']
  version = tuple(version[0:3])
  if version != (2, 6, 0):
    client.__class__ = GestapoMongoClient


class Meta(infinit.oracles.meta.server.Meta):

  def __init__(self, *args, **kwargs):
    # Force admin to avoid checking certificate.
    if not 'force_admin' in kwargs:
      kwargs['force_admin'] = True
    super().__init__(*args, **kwargs)

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
            self.__mandrill._Mandrill__emails.append((email, human))
            found = True
            break
        if not found:
          raise Exception('missing merge_vars for %s' % email)
      return [
        {
          'email': to['email'],
          'status': 'sent',
          'reject_reason': None,
          '_id': str(uuid.uuid4()),
        }
        for to in message['to']]

  @property
  def messages(self):
    return Mandrill.Messages(self)

class DummyEmailer:

  def __init__(self):
    self.__emails = []

  @property
  def emails(self):
    res = self.__emails
    self.__emails = []
    return res

  def send_template(self, template, recipients):
    for recipient in recipients:
      self.__emails.append(
        (recipient['email'], recipient['vars'], template))

def user_register(meta, email):
  user = meta.user_register(email, '*' * 64, 'Foo Bar')
  # FIXME: internal code see users as from-the-database objects, hence
  # _id. Remove when meta uses rich objects.
  user['_id'] = user['id']
  user.setdefault('devices', [])
  return user

def transaction_create(meta, sender, recipient, files = ['foobar'],
                       initialize = True, size = 42):
  tid = meta.transaction_create(
    sender, recipient, files, 1, size, False, 'device')
  tid = tid['created_transaction_id']
  if initialize:
    meta._transaction_update(tid, statuses['initialized'],
                             'device', None, sender)
  return tid

def check_mail(mails, user, template):
  assertEq(len(mails), 1)
  mail = mails[0]
  assertEq(mail[0], user)
  assertEq(mail[2], template)
  content = mail[1]
  assertEq(content['user']['email'], user)

def check_mail_transaction(mails, sender, recipient):
  assert len(mails) == 1
  mail = mails[0]
  assert mail[0] == recipient
  content = mail[1]
  assertEq(content['sender']['email'], sender)
  assertEq(content['recipient']['email'], recipient)
  assert 'avatar' in content['sender']
  assert 'fullname' in content['sender']
  assert 'files' in content['transaction']
  assert 'id' in content['transaction']
  assert 'key' in content['transaction']
  assert 'message' in content['transaction']

def check_no_mail(mails):
  if len(mails) > 0:
    raise Exception(
      'unexpected email to %s: %s' % (mails[0][0], mails[0][2]))

def assertEq(a, b):
  if a != b:
    raise AssertionError('%r != %r' % (a, b))
