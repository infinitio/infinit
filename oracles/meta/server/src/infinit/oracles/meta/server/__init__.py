#!/usr/bin/python3

# Load this FIRST to ensure we load our own openssl. Otherwise the
# system one will be loaded through hashlib, by bottle for instance.
import papier

import bottle
import datetime
import decorator
import elle.log
import inspect
import mako.lookup
import pymongo
import pymongo.collection
import pymongo.database
import pymongo.errors
import re

from .plugins.certification import Plugin as CertificationPlugin
from .plugins.failure import Plugin as FailurePlugin
from .plugins.jsongo import Plugin as JsongoPlugin
from .plugins.response import Plugin as ResponsePlugin
from .plugins.response import response
from .plugins.session import Plugin as SessionPlugin
from .plugins.watermark import Plugin as WatermarkPlugin
from infinit.oracles.meta import error

from .utils import api, hash_pasword, require_admin, require_logged_in

from . import apertus
from . import cloud_buffer_token
from . import device
from . import invitation
from . import link_generation
from . import mail
from . import notifier
from . import root
from . import transaction
from . import trophonius
from . import user
from . import waterfall

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.Meta'

def reconnect_mongocall(f):
  def result(f, *args, **kwargs):
    i = 0
    while True:
      try:
        return f(*args, **kwargs)
      except pymongo.errors.AutoReconnect as e:
        if i < 5:
          i += 1
        import time
        elle.log.warn('database unreachable, will try autoreconect in %ss: %s' % (i, e))
        time.sleep(pow(2, i))
  return decorator.decorator(result, f)

class ReconnectingDatabase(pymongo.database.Database):

  def __getattr__(self, key):
    res = pymongo.database.Database.__getattr__(self, key)
    res.__class__ = ReconnectingCollection
    return res

  def __getitem__(self, key):
    res = pymongo.database.Database.__getitem__(self, key)
    res.__class__ = ReconnectingCollection
    return res

class ReconnectingCollection(pymongo.collection.Collection):
  pass

for m in set(m for m in dir(pymongo.collection.Collection)
             if not m.startswith('_')
             and callable(getattr(pymongo.collection.Collection, m))):
  def closure(m):
    original = getattr(pymongo.collection.Collection, m)
    @reconnect_mongocall
    def wrapper(*args, **kwargs):
      res = original(*args, **kwargs)
      if isinstance(res, pymongo.cursor.Cursor):
        res.__class__ = ReconnectingCursor
      return res
    wrapper.__name__ = m
    setattr(ReconnectingCollection, m, wrapper)
  closure(m)


class ReconnectingCursor(pymongo.cursor.Cursor):
  pass

for m in set(m for m in dir(pymongo.cursor.Cursor)
             if not m.startswith('_')
             and callable(getattr(pymongo.cursor.Cursor, m))
             or m in ['__next__']):
  def closure(m):
    original = getattr(pymongo.cursor.Cursor, m)
    @reconnect_mongocall
    def wrapper(*args, **kwargs):
      return original(*args, **kwargs)
    wrapper.__name__ = m
    setattr(ReconnectingCursor, m, wrapper)
  closure(m)

class Meta(bottle.Bottle,
           root.Mixin,
           user.Mixin,
           transaction.Mixin,
           device.Mixin,
           trophonius.Mixin,
           apertus.Mixin,
           waterfall.Waterfall,
           link_generation.Mixin,
         ):

  def __init__(
      self,
      mongo_host = None,
      mongo_port = None,
      mongo_replica_set = None,
      enable_emails = True,
      enable_invitations = True,
      trophonius_expiration_time = 90, # in sec
      apertus_expiration_time = 90, # in sec
      unconfirmed_email_leeway = 604800, # in sec, 7 days.
      daily_summary_hour = 18, #in sec.
      email_confirmation_cooldown = datetime.timedelta(weeks = 1),
      aws_region = None,
      aws_buffer_bucket = None,
      aws_link_bucket = None,
      force_admin = False,
      debug = False,
  ):
    import os
    system_logger = os.getenv("META_LOG_SYSTEM")
    if system_logger is not None:
      elle.log.set_logger(elle.log.SysLogger(system_logger))
    self.__force_admin = force_admin
    if self.__force_admin:
      elle.log.warn('%s: running in force admin mode' % self)
    super().__init__()
    if mongo_replica_set is not None:
      with elle.log.log(
          '%s: connect to MongoDB replica set %s' % (self, mongo_replica_set)):
        self.__mongo = \
          pymongo.MongoReplicaSetClient(','.join(mongo_replica_set),
                                        replicaSet = 'fist-meta')
    else:
      with elle.log.log(
          '%s: connect to MongoDB on %s:%s' % (self, mongo_host, mongo_port)):
        db_args = {}
        if mongo_host is not None:
          db_args['host'] = mongo_host
        if mongo_port is not None:
          db_args['port'] = mongo_port
        self.__mongo = pymongo.MongoClient(**db_args)
    self.__database = self.__mongo.meta
    self.__database.__class__ = ReconnectingDatabase
    self.__set_constraints()
    self.catchall = debug
    bottle.debug(debug)
    # Plugins.
    self.install(ResponsePlugin())
    self.install(WatermarkPlugin())
    self.install(FailurePlugin())
    self.__sessions = SessionPlugin(self.__database, 'sessions')
    self.install(self.__sessions)
    self.install(JsongoPlugin())
    self.install(CertificationPlugin())
    # Configuration.
    self.ignore_trailing_slash = True
    # Routing.
    for function in api.functions:
      self.__register(function)
    # Notifier.
    self.notifier = notifier.Notifier(self.__database)
    # Share
    share_path = os.path.realpath('/'.join(__file__.split('/')[:-7]))
    share_path = '%s/share/infinit/meta/server/' % share_path
    self.__share_path = share_path
    # Resources
    self.__resources_path = '%s/resources' % share_path
    # Templates
    self.__mako_path = '%s/templates' % share_path
    self.__mako = mako.lookup.TemplateLookup(
      directories = [self.__mako_path]
    )
    # Could be cleaner.
    self.mailer = mail.Mailer(active = enable_emails)
    self.invitation = invitation.Invitation(active = enable_invitations)
    self.trophonius_expiration_time = int(trophonius_expiration_time)
    self.apertus_expiration_time = int(apertus_expiration_time)
    self.unconfirmed_email_leeway = int(unconfirmed_email_leeway)
    self.daily_summary_hour = int(daily_summary_hour)
    self.email_confirmation_cooldown = email_confirmation_cooldown
    if aws_region is None:
      aws_region = cloud_buffer_token.aws_default_region
    self.aws_region = aws_region
    if aws_buffer_bucket is None:
      aws_buffer_bucket = cloud_buffer_token.aws_default_buffer_bucket
    self.aws_buffer_bucket = aws_buffer_bucket
    if aws_link_bucket is None:
      aws_link_bucket = cloud_buffer_token.aws_default_link_bucket
    self.aws_link_bucket = aws_link_bucket
    waterfall.Waterfall.__init__(self)

  def __set_constraints(self):
    #---------------------------------------------------------------------------
    # Users
    #---------------------------------------------------------------------------
    # - Default search.
    self.__database.users.ensure_index([("fullname", 1), ("handle", 1)])
    # - Email confirmation.
    self.__database.users.ensure_index([("email_confirmation_hash", 1)],
                                       unique = True, sparse = True)
    # - Lost password.
    self.__database.users.ensure_index([("reset_password_hash", 1)],
                                       unique = True, sparse = True)
    # - Midnight cron.
    self.__database.users.ensure_index([("_id", 1), ("last_connection", 1)])
    # - Mailchimp userbase.
    self.__database.users.ensure_index([("_id", 1), ("os", 1)])

    #---------------------------------------------------------------------------
    # Devices
    #---------------------------------------------------------------------------
    # - Default search.
    self.__database.devices.ensure_index([("id", 1), ("owner", 1)],
                                         unique = True)
    # - Trophonius disconnection.
    self.__database.devices.ensure_index([("trophonius", 1)],
                                         unique = False)

    #---------------------------------------------------------------------------
    # Transactions
    #---------------------------------------------------------------------------
    # - ??
    self.__database.transactions.ensure_index('ctime')

    # - Midnight cron.
    self.__database.transactions.ensure_index([("mtime", 1),
                                               ('status', 1)])
    # - Default transaction search.
    self.__database.transactions.ensure_index([("sender_id", 1),
                                               ("recipient_id", 1),
                                               ('status', 1),
                                               ('mtime', 1)])
    # - Download by link transaction hash.
    self.__database.transactions.ensure_index([('transaction_hash', 1)],
                                              unique = True, sparse = True)

    #---------------------------------------------------------------------------
    # Link Generation
    #---------------------------------------------------------------------------
    # - Download by link history.
    self.__database.links.ensure_index([('sender_id', pymongo.ASCENDING),
                                        ('creation_time', pymongo.DESCENDING)])

    # - Download by link hash.
    self.__database.links.ensure_index([('hash', 1)],
                                       unique = True, sparse = True)

  def __register(self, method):
    rule = method.__route__
    elle.log.debug('%s: register route %s' % (self, rule))
    # Introspect method.
    spec = inspect.getfullargspec(method.__underlying_method__)
    del spec.args[0] # remove self
    import itertools
    defaults = spec.defaults or []
    spec_args = dict((name, default)
                     for name, default
                     in itertools.zip_longest(
                       reversed(spec.args),
                       reversed([True] * len(defaults))))
    for arg in re.findall('<(\\w*)(?::\\w*(?::[^>]*)?)?>', rule):
      if arg in spec_args:
        del spec_args[arg]
      elif spec.varkw is None:
        raise AssertionError(
          'Rule %r yields %r but %r does not accept it' % (rule, arg, method))
    # Callback.
    def callback(*args, **kwargs):
      arguments = dict(spec_args)
      def explode(d):
        if d is None:
          return
        for key in dict(arguments):
          if key in d:
            kwargs[key] = d[key]
            del d[key]
            del arguments[key]
        if len(d) > 0:
          if spec.varkw is not None:
            kwargs.update(d)
          else:
            key = iter(d.keys()).__next__()
            self.bad_request('unexpected JSON keys: %r' % key)
      try:
        explode(bottle.request.json)
      except ValueError:
        self.bad_request('invalid JSON')
      explode(bottle.request.query)
      for argument, default in arguments.items():
        if not default:
          self.bad_request('missing argument: %r' % argument)
      return method(self, *args, **kwargs)
    # Add route.
    route = bottle.Route(app = self,
                         rule = rule,
                         method = method.__method__,
                         callback = callback)
    self.add_route(route)

  @property
  def mailer(self):
    return self.__mailer

  @mailer.setter
  def mailer(self, mailer):
    self.__mailer = mailer
    if self.__mailer is not None:
      self.__mailer.meta = self

  @property
  def remote_ip(self):
    return bottle.request.environ.get('REMOTE_ADDR')

  def fail(self, *args, **kwargs):
    FailurePlugin.fail(*args, **kwargs)

  def success(self, res = {}):
    assert isinstance(res, dict)
    res['success'] = True
    return res

  @property
  def database(self):
    assert self.__database is not None
    return self.__database

  # Make it accessible from user.
  @property
  def sessions(self):
    assert self.__sessions is not None
    return self.__sessions

  def abort(self, message):
    response(500, message)

  def forbidden(self, message = None):
    response(403, message)

  def gone(self, message = None):
    response(410, message)

  def not_found(self, message = None):
    response(404, message)

  def bad_request(self, message = None):
    response(400, message)

  @api('/js/<filename:path>')
  def static_javascript(self, filename):
    return self.__static('js/%s' % filename)

  @api('/css/<filename:path>')
  def static_css(self, filename):
    return self.__static('css/%s' % filename)

  @api('/images/<filename:path>')
  def static_images(self, filename):
    return self.__static('images/%s' % filename)

  @api('/favicon.ico')
  def static_css(self):
    return self.__static('favicon.ico')

  def __static(self, filename):
    return bottle.static_file(
      filename, root = self.__resources_path)

  @property
  def admin(self):
    source = bottle.request.environ.get('REMOTE_ADDR')
    force = self.__force_admin or source == '127.0.0.1'
    return force or bottle.request.certificate in [
      'antony.mechin@infinit.io',
      'baptiste.fradin@infinit.io',
      'christopher.crone@infinit.io',
      'gaetan.rochel@infinit.io',
      'julien.quintard@infinit.io',
      'matthieu.nottale@infinit.io',
      'patrick.perlmutter@infinit.io',
      'quentin.hocquet@infinit.io',
    ]

  @property
  def user_agent(self):
    from bottle import request
    return request.environ.get('HTTP_USER_AGENT')

  @property
  def user_version(self):
    with elle.log.debug('%s: get user version' % self):
      import re
      # Before 0.8.11, user agent was empty.
      if self.user_agent is None or len(self.user_agent) == 0:
        return (0, 8, 10)
      pattern = re.compile('^MetaClient/(\\d+)\\.(\\d+)\\.(\\d+)')
      match = pattern.search(self.user_agent)
      if match is None:
        elle.log.debug('can\'t extract version from user agent %s' %
                       self.user_agent)
        # Website.
        return (0, 0, 0)
      else:
        return tuple(map(int, match.groups()))

  def user_by_id_or_email(self, id_or_email):
    id_or_email = id_or_email.lower()
    if '@' in id_or_email:
      return self.user_by_email(id_or_email, ensure_existence = False)
    else:
      return self._user_by_id(bson.ObjectId(id_or_email),
                              ensure_existence = False)
