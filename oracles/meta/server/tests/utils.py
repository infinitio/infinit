#!/usr/bin/env python3

import infinit.oracles.meta.server
from infinit.oracles.meta.server.mail import Mailer
from infinit.oracles.meta.server.invitation import Invitation
from infinit.oracles.meta.server import transaction_status
from infinit.oracles.meta import version

import http.cookies
import os
import bson
import pymongo
import sys

root = os.path.realpath(os.path.dirname(__file__))
sys.path.append(root + '/../src')
sys.path.append(root + '/../../../mongobox')
sys.path.append(root + '/../../../bottle')

import httplib2
import json
import mongobox

import bottle

from uuid import uuid4, UUID

class HTTPException(Exception):

  def __init__(self, status, method, url, body, content):
    self.status = int(status)
    self.content = content
    super().__init__('status %s with %s on /%s with body %s' % \
                     (status, method, url, body))

class Client:

  def __init__(self, meta):
    self.__cookies = None
    self.__meta_port = meta._Meta__server.port
    self.user_agent = 'MetaClient/' + version.version

  def __get_cookies(self, headers):
    cookies = headers.get('set-cookie', None)
    if cookies is not None:
      self.__cookies = http.cookies.SimpleCookie(cookies)

  def __set_cookies(self, headers):
    if self.__cookies is not None:
      headers['Cookie'] = self.__cookies.output()

  @property
  def cookie(self):
    return self.__cookies

  def __convert_result(self, url, method, body, headers, content):
    status = headers['status']
    if headers['content-type'] == 'application/json':
      content = json.loads(content.decode())
    elif headers['content-type'] == 'text/plain':
      content = content.decode()
    else:
      content = content
    if status != '200':
      raise HTTPException(status, method, url, body, content)
    else:
      return content

  def request(self, url, method, body):
    h = httplib2.Http()
    uri = "http://localhost:%s/%s" % (self.__meta_port, url)
    headers = {}
    headers['user-agent'] = self.user_agent
    if body is not None and isinstance(body, dict):
      headers['Content-Type'] = 'application/json'
      body = json.dumps(body)
    self.__set_cookies(headers)
    resp, content = h.request(uri,
                              method,
                              body = body,
                              headers = headers)
    self.__get_cookies(resp)
    return self.__convert_result(url, method, body, resp, content)

  def post(self, url, body = None):
    return self.request(url, 'POST', body)

  def get(self, url, body = None):
    return self.request(url, 'GET', body)

  def put(self, url, body = None):
    return self.request(url, 'PUT', body)

  def delete(self, url, body = None):
    return self.request(url, 'DELETE', body)

class Trophonius(Client):

  def __init__(self, meta):
    super().__init__(meta)
    self.__uuid = str(uuid4())
    self.__users = {}
    self.__args = {
      'port': 23456,
      'port_client': 23457,
      'port_client_ssl': 23458,
    }

  def __enter__(self):
    res = self.put('trophonius/%s' % self.__uuid, self.__args)
    assert res['success']
    return self

  def __exit__(self, *args, **kwargs):
    res = self.delete('trophonius/%s' % self.__uuid)
    assert res['success']

  def connect_user(self, user):
    user_id = str(user.id)
    res = user.put('trophonius/%s/users/%s/%s' % \
                   (self.__uuid, user_id, str(user.device_id)))
    assert res['success']
    self.__users.setdefault(user_id, [])
    self.__users[user_id] += str(user.device_id)

  def disconnect_user(self, user):
    assert user.device_id in self.__users[user.id]
    res = user.delete('trophonius/%s/users/%s/%s' % \
                      (self.__uuid, str(user.id), str(user.device_id)))
    assert res['success']
    self.__users[user.id].remove(user.device_id)

class NoOpMailer(Mailer):

  def __init__(self, op = None):
    self.__sent = 0
    super().__init__(True)

  def _Mailer__send(self, message):
    self.__sent += 1
    self.view_message(message)

  def _Mailer__send_template(self, template_name, message):
    self.__sent += 1
    self.template_message(template_name, message)

  def template_message(self, template_name, message):
    self.view_message(message)
    pass

  def view_message(self, message):
    pass

  @property
  def sent(self):
    return self.__sent

class NoOpInvitation(Invitation):

  class FakeMailsnake:

    def __init__(self):
      self.__lists = {}
      self.__unsubscribed = {}

    def userbase(self, name):
      return self.__lists.setdefault(name, set())

    def unsubscribed(self, name):
      return self.__unsubscribed.setdefault(name, set())

    def listSubscribe(self, id, email_address, double_optin):
      if email_address in self.unsubscribed(id):
        self.unsubscribed(id).remove(email_address)
      self.userbase(id).add(email_address)
      return True

    def listUnsubscribe(self, id, email_address):
      try:
        self.userbase(id).remove(email_address)
        self.unsubscribed(id).add(email_address)
        return True
      except KeyError:
        return False

    def listMemberInfo(self, id, email_address):
      if email_address in self.unsubscribed(id):
        return {
          "success": 1,
          "data": [{"status": "unsubscribed"}]
        }
      if email_address in self.userbase(id):
        return {
          "success": 1,
          "data": [{"status": "subscribed"}]
        }
      else:
        return {
          "success": 1,
          "data": []
        }

    def listMembers(self, id):
      return {"data": self.userbase(id)}

  def __init__(self):
    super().__init__(True)
    self.ms = NoOpInvitation.FakeMailsnake()
    # XXX: Reset lists to secure the tests.
    # self.lists = {}

class Meta:

  def __init__(self,
               enable_emails = False,
               enable_invitations = False,
               force_admin = False, **kw):
    self.__mongo = mongobox.MongoBox()
    self.__server = bottle.WSGIRefServer(port = 0)
    self.__database = None
    self.__client = None
    self.__enable_emails = enable_emails
    self.__enable_invitations = enable_invitations
    self.__force_admin = force_admin
    self.__meta = None
    self.__meta_args = kw

  def __enter__(self):
    self.__mongo.__enter__()
    client = pymongo.MongoClient(port = self.__mongo.port)
    self.__database = client.meta
    def run():
      try:
        self.__meta = infinit.oracles.meta.server.Meta(
          mongo_port = self.__mongo.port,
          enable_emails = self.__enable_emails,
          enable_invitations = self.__enable_invitations,
          force_admin = self.__force_admin,
          **self.__meta_args)
        self.__meta.mailer = NoOpMailer()
        self.__meta.invitation = NoOpInvitation()
        self.__meta.catchall = False
        bottle.run(app = self.__meta,
                   quiet = True,
                   server = self.__server)
      except KeyboardInterrupt:
        pass
    import threading
    self.__thread = threading.Thread(target = run)
    self.__thread.daemon = True
    self.__thread.start()
    while self.__server.port == 0 and self.__thread.is_alive():
      import time
      time.sleep(.1)
    return self

  @property
  def mailer(self):
    return self.__meta.mailer

  @mailer.setter
  def mailer(self, mailer):
    assert self.__meta is not None
    self.__meta.mailer = mailer

  @property
  def invitation(self):
    return self.__meta.invitation

  @invitation.setter
  def invitation(self, invitation):
    assert self.__meta is not None
    self.__meta.invitation = invitation

  @property
  def notifier(self):
    return self.__meta.notifier

  @notifier.setter
  def notifier(self, notifier):
    assert self.__meta is not None
    self.__meta.notifier = notifier

  def __exit__(self, *args, **kwargs):
    self.__mongo.__exit__(*args, **kwargs)

  def post(self, url, body = None):
    return self.client.post(url, body)

  def get(self, url, body = None):
    return self.client.get(url, body)

  def put(self, url, body = None):
    return self.client.put(url, body)

  def delete(self, url, body = None):
    return self.client.delete(url, body)

  def create_user(self,
                  email,
                  fullname = None,
                  password = '0' * 64,
                  source = None,
                  ):
    res = self.post('user/register',
                    {
                      'email': email,
                      'password': password,
                      'fullname': fullname or email.split('@')[0],
                      'source': source,
                    })
    assert res['success']
    return password

  @property
  def database(self):
    return self.__database

  @property
  def client(self):
    if self.__client is None:
      self.__client = Client(self)
    return self.__client

def throws(f, expected = None):
  guard = None
  if isinstance(expected, int):
    guard = lambda e: e.status == expected
    expected = HTTPException
  if expected is None:
    expected = BaseException
  try:
    f()
  except expected as e:
    if guard is not None:
      guard(e)
  else:
    raise Exception('exception expected')

def random_email(N = 10):
  import random
  from time import time
  random.seed(time())
  import string
  return ''.join(random.choice(string.ascii_lowercase + string.digits) for _ in range(N))

class User(Client):

  def __init__(self,
               meta,
               email = None,
               device_name = 'device',
               **kwargs):
    super().__init__(meta)

    self.email = email is not None and email or random_email() + '@infinit.io'
    self.password = meta.create_user(self.email,
                                     **kwargs)
    self.id = meta.get('user/%s/view' % self.email)['_id']
    self.device_id = uuid4()

  @property
  def login_parameters(self):
    return {
      'email': self.email,
      'password': self.password,
      'device_id': str(self.device_id),
    }

  @property
  def data(self):
    res = self.get('user/self')
    assert res['success']
    return res

  def login(self, device_id = None, **kw):
    if device_id is not None:
      self.device_id = device_id
    params = self.login_parameters
    params.update(kw)
    params.update({'pick_trophonius': False})
    res = self.post('login', params)
    assert res['success']
    assert res['device_id'] == str(self.device_id)
    return res

  def logout(self):
    res = self.post('logout', {})
    assert res['success']

  @property
  def device(self):
    assert self.device_id is not None
    return self.get('device/%s/view' % str(self.device_id))

  @property
  def device_name(self):
    return self.device['name']

  @property
  def swaggers(self):
    return self.get('user/swaggers')['swaggers']

  @property
  def favorites(self):
    return self.data['favorites']

  @property
  def logged_in(self):
    try:
      res = self.data
      assert res['success']
      assert str(self.device_id) in res['devices']
      return True
    except HTTPException as e:
      assert e.status == 403
      return False

  @property
  def _id(self):
    return bson.ObjectId(self.data['_id'])

  @property
  def devices(self):
    return self.get('devices')['devices']

  @property
  def avatar(self):
    return self.get('user/%s/avatar' % self.id)

  @property
  def connected(self):
    return self.get('user/%s/connected' % self.id)['connected']

  @property
  def identity(self):
    return self.data['identity']

  @property
  def fullname(self):
    return self.data['fullname']

  @property
  def transactions(self):
    res = self.get('transactions')
    assert res['success']
    return res['transactions']

  @property
  def connected_on_device(self, device_id = None):
    if device_id is None:
      device_id = self.device_id
    return self.get('device/%s/%s/connected' % (self.id, str(device_id)))['connected']

  def __eq__(self, other):
    if isinstance(other, User):
      return self.email == other.email
    return NotImplemented

  def sendfile(self,
               recipient_id,
               files = ['50% off books.pdf',
                        'a file with strange encoding: é.file',
                        'another file with no extension'],
               total_size = 2164062,
               message = 'no message',
               is_directory = False,
               device_id = None,
               initialize = False,
               ):
    if device_id is None:
      device_id = self.device_id

    transaction = {
      'id_or_email': recipient_id,
      'files': files,
      'files_count': len(files),
      'total_size': total_size,
      'message': message,
      'is_directory': is_directory,
      'device_id': str(device_id),
    }

    res = self.post('transaction/create', transaction)
    if initialize:
      self.transaction_update(res['created_transaction_id'],
                              transaction_status.INITIALIZED)
    return transaction, res

  def transaction_update(self, transaction, status):
    self.post('transaction/update',
              {
                'transaction_id': transaction,
                'status': status,
                'device_id': str(self.device_id),
                'device_name': self.device_name,
              })
