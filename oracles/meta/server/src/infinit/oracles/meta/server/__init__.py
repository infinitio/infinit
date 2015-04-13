#!/usr/bin/python3

# Load this FIRST to ensure we load our own openssl. Otherwise the
# system one will be loaded through hashlib, by bottle for instance.
import papier

import bson
import bottle
import bson
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

import infinit.oracles.emailer

from .plugins.certification import Plugin as CertificationPlugin
from .plugins.failure import Plugin as FailurePlugin
from .plugins.fatal_emails import Plugin as FatalPlugin
from .plugins.jsongo import Plugin as JsongoPlugin
from .plugins.response import Plugin as ResponsePlugin
from .plugins.response import response
from .plugins.session import Plugin as SessionPlugin
from .plugins.watermark import Plugin as WatermarkPlugin
from infinit.oracles.meta import error

from .utils import api, require_admin, require_logged_in, key

from . import apertus
from . import cloud_buffer_token
from . import device
from . import features
from . import invitation
from . import link_generation
from . import mail
from . import notifier
from . import root
from . import transaction
from . import trophonius
from . import user
from . import waterfall
from . import facebook
from . import shortener

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.Meta'

def reconnect(original):
  def result(f, *args, **kwargs):
    import sys
    i = 0
    while True:
      try:
        return f(*args, **kwargs)
      except pymongo.errors.AutoReconnect as e:
        import time
        delay = pow(2, 1 + i / 2)
        elle.log.warn('database unreachable, will try autoreconect in %ss: %s' % (delay, e))
        time.sleep(delay)
        if i < 10:
          i += 1
  return decorator.decorator(result, original)

pymongo.cursor.Cursor._Cursor__send_message = reconnect(pymongo.cursor.Cursor._Cursor__send_message)
pymongo.connection.Connection._send_message = reconnect(pymongo.connection.Connection._send_message)
pymongo.connection.Connection._send_message_with_response = reconnect(pymongo.connection.Connection._send_message_with_response)
pymongo.MongoReplicaSetClient._send_message = reconnect(pymongo.MongoReplicaSetClient._send_message)


class Meta(bottle.Bottle,
           root.Mixin,
           user.Mixin,
           transaction.Mixin,
           device.Mixin,
           features.Mixin,
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
      shorten_ghost_profile_url = True,
      aws_region = None,
      aws_buffer_bucket = None,
      aws_link_bucket = None,
      gcs_region = None,
      gcs_buffer_bucket = None,
      gcs_link_bucket = None,
      force_admin = None,
      debug = False,
      zone = None,
      production = False,
      facebook_domain = "https://graph.facebook.com",
      emailer = None,
      stripe_api_key = None,
  ):
    self.__production = production
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
          pymongo.MongoReplicaSetClient(
            ','.join(mongo_replica_set),
            replicaSet = 'fist-meta',
            socketTimeoutMS = 3000,
            connectTimeoutMS = 1000,
          )
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
    self.__set_constraints()
    self.catchall = debug
    bottle.debug(debug)
    # Plugins.
    self.install(FatalPlugin(self.report_fatal_error))
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
    api.register(self)
    # Notifier.
    self.notifier = notifier.Notifier(self.__database,
                                      production = self.production)
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
    self.shorten_ghost_profile_url = shorten_ghost_profile_url
    self.shortener = shortener.ShortSwitch()
    if aws_region is None:
      aws_region = cloud_buffer_token.aws_default_region
    self.aws_region = aws_region
    if aws_buffer_bucket is None:
      aws_buffer_bucket = cloud_buffer_token.aws_default_buffer_bucket
    self.aws_buffer_bucket = aws_buffer_bucket
    if aws_link_bucket is None:
      aws_link_bucket = cloud_buffer_token.aws_default_link_bucket
    self.aws_link_bucket = aws_link_bucket
    if gcs_region is None:
      gcs_region = cloud_buffer_token_gcs.gcs_default_region
    self.gcs_region = gcs_region
    if gcs_buffer_bucket is None:
      gcs_buffer_bucket = cloud_buffer_token_gcs.gcs_default_buffer_bucket
    self.gcs_buffer_bucket = gcs_buffer_bucket
    if gcs_link_bucket is None:
      gcs_link_bucket = cloud_buffer_token_gcs.gcs_default_link_bucket
    self.gcs_link_bucket = gcs_link_bucket
    waterfall.Waterfall.__init__(self)
    self.__zone = zone
    # Facebook.
    self.facebook = facebook.FacebookGraph(facebook_domain)
    # Emailing
    self.__emailer = emailer or infinit.oracles.emailer.NoopEmailer()
    self.__stripe_api_key = stripe_api_key

  @property
  def emailer(self):
    return self.__emailer

  @property
  def production(self):
    return self.__production

  @property
  def stripe_api_key(self):
    return self.__stripe_api_key

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
    # - register and user search
    self.__database.users.ensure_index([("accounts.id", 1)],
                                       unique = True, sparse = True)
    # - Auxiliary emails.
    # Sparse because users may have no pending_auxiliary_emails field.
    self.__database.users.ensure_index([("pending_auxiliary_emails.hash", 1)], unique = True, sparse = True)
    # - lw_handle
    self.__database.users.ensure_index([('lw_handle', 1)],
                                       unique = False)
    self.__database.users.ensure_index([('devices.id', 1)],
                                       unique = False)
    # - Ghost code.
    self.__database.users.ensure_index([('ghost_code', 1)], unique = True, sparse = True)

    # - Push token.
    self.__database.users.ensure_index(
      [('devices.push_token', 1)],
      unique = True,
      sparse = True)

    # - Login.
    self.__database.users.ensure_index([('email', 1)],
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
    # - Finding transaction by peer
    self.__database.transactions.ensure_index(
      [('sender_id', pymongo.ASCENDING),
       ('ctime', pymongo.ASCENDING)])
    self.__database.transactions.ensure_index(
      [('recipient_id', pymongo.ASCENDING),
       ('ctime', pymongo.ASCENDING)])
    # - Listing transactions for one user. More efficient than using status
    self.__database.transactions.ensure_index(
      [('involved', pymongo.ASCENDING),
      ('mtime', pymongo.DESCENDING)])

    #---------------------------------------------------------------------------
    # Link Generation
    #---------------------------------------------------------------------------
    # - Download by link history.
    self.__database.links.ensure_index([('sender_id', pymongo.ASCENDING),
                                        ('creation_time', pymongo.DESCENDING)])

    # - Download by link hash.
    self.__database.links.ensure_index([('hash', 1)],
                                       unique = True, sparse = True)

    # Sessions
    self.__database.sessions.ensure_index([('device', 1)],
                                          unique = False)

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

  def bad_request(self, message = None):
    response(400, message)

  def conflict(self, message = None):
    response(409, message)

  def unavailable(self, message = None):
    response(503, message)

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
    force = self.__force_admin or \
      (self.__force_admin is not False and source == '127.0.0.1')
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
  def logged_in(self):
    return self.user is not None

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

  @property
  def user_gcs_enabled(self):
    # XXX: Force gcs desactivation on 0.9.33
    if self.user_version < (0, 9, 34):
      return False
    if self.device_mobile:
      return True
    return self.user['features'].get('gcs_enabled', 'false') != 'false'

  @property
  def device_mobile(self):
    device = self.current_device
    if device is None or 'os' not in device:
      return None
    return device['os'] in ('iOS', 'Android')

  def report_fatal_error(self, route, exception):
    import traceback
    e = exception
    import socket
    hostname = socket.getfqdn()
    with elle.log.log(
        '%s: send email for fatal error: %s' % (self, e)):
      args = {
        'backtrace': ''.join(
          traceback.format_exception(type(e), e, None)),
        'hostname': hostname,
        'route': route,
        'session': bottle.request.session,
        'url': bottle.request.url,
        'user': self.user,
      }
      self.mailer.send(to = 'infrastructure@infinit.io',
                       fr = 'root@%s' % hostname,
                       subject = 'Meta: fatal error: %s' % e,
                       body = '''\
Error while querying %(route)s:

URL: %(url)s
User: %(user)s
Session: %(session)s

%(backtrace)s''' % args)

  def report_short_link_problem(self, retries):
    with elle.log.log('unable to create short link after %s tries' % retries):
      self.mailer.send(
        to = 'infrastructure@infinit.io',
        fr = 'infrastructure@infinit.io',
        subject = ('Meta: unable to create unique short link after %s tries' %
                   retries),
        body = 'No body')

  @property
  def now(self):
    return datetime.datetime.utcnow()

  def check_key(self, k):
    if k is None or k != key(bottle.request.path):
      self.forbidden()

  def url_absolute(self, url = ''):
    if not url.startswith('/'):
      url = '/' + url
    meta = '%s://%s' % bottle.request.urlparts[0:2]
    return meta + url

  def email_user_vars(self, user):
    return infinit.oracles.emailer.user_vars(
      user, self.url_absolute())

  def email_transaction_vars(self, transaction, user):
    return infinit.oracles.emailer.transaction_vars(
      transaction, user, self.url_absolute())

  # Shorten url.
  def shorten(self, url):
    if self.shorten_ghost_profile_url:
      try:
        return self.shortener.shorten(url)
      except shortener.ShortenerException as e:
        self.mailer.send(
          to = 'infrastructure@infinit.io',
          fr = 'infrastructure@infinit.io',
          subject = ('Meta: Unable to shorten using %s' % self.shortener),
          body = 'Exception: %s' % e)
    return url
