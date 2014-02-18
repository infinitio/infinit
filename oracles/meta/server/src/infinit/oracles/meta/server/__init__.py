#!/usr/bin/python3

# Load this FIRST to ensure we load our own openssl. Otherwise the
# system one will be loaded through hashlib, by bottle for instance.
import papier

import bottle
import elle.log
import inspect
import mako.lookup
import papier
import pymongo
import re

from .plugins.jsongo import Plugin as JsongoPlugin
from .plugins.failure import Plugin as FailurePlugin
from .plugins.session import Plugin as SessionPlugin
from .plugins.certification import Plugin as CertificationPlugin

from infinit.oracles.meta import error

from .utils import api, hash_pasword, require_logged_in

from . import apertus
from . import device
from . import invitation
from . import mail
from . import metrics
from . import notifier
from . import root
from . import transaction
from . import trophonius
from . import user
from . import waterfall

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.Meta'

class Meta(bottle.Bottle,
           root.Mixin,
           user.Mixin,
           transaction.Mixin,
           device.Mixin,
           trophonius.Mixin,
           apertus.Mixin,
           waterfall.Mixin,
           metrics.Metrics,
         ):

  def __init__(self,
               mongo_host = None,
               mongo_port = None,
               enable_emails = True,
               trophonius_expiration_time = 300, # in sec
               apertus_expiration_time = 300 # in sec
               ):
    import os
    system_logger = os.getenv("META_LOG_SYSTEM")
    if system_logger is not None:
      elle.log.set_logger(elle.log.SysLogger(system_logger))
    super().__init__()
    db_args = {}
    if mongo_host is not None:
      db_args['host'] = mongo_host
    if mongo_port is not None:
      db_args['port'] = mongo_port
    with elle.log.log('%s: connect to MongoDB on %s:%s' %
                      (self, mongo_host, mongo_port)):
      self.__database = pymongo.MongoClient(**db_args).meta
      self.__set_constraints()
    self.catchall = False
    # Plugins.
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
    share_path = '/'.join(__file__.split('/')[:-9])
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
    self.invitation = invitation.Invitation(active = enable_emails)
    self.trophonius_expiration_time = trophonius_expiration_time
    self.apertus_expiration_time = apertus_expiration_time
    metrics.Metrics.__init__(self)

  def __set_constraints(self):
    self.__database.devices.ensure_index([("id", 1), ("owner", 1)], unique = True)
    self.__database.transactions.ensure_index('ctime')

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
    for arg in re.findall('<(\\w*)(?::\\w*(?::\\w*)?)?>', rule):
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
            bottle.abort(400, 'unexpected JSON keys: %r' % key)
      try:
        explode(bottle.request.json)
      except ValueError:
        bottle.abort(400, 'invalid JSON')
      explode(bottle.request.query)
      for argument, default in arguments.items():
        if not default:
          bottle.abort(400, 'missing argument: %r' % argument)
      return method(self, *args, **kwargs)
    # Add route.
    route = bottle.Route(app = self,
                         rule = rule,
                         method = method.__method__,
                         callback = callback)
    self.add_route(route)

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
    bottle.abort(500, message)

  def forbiden(self):
    bottle.abort(403)

  def not_found(self, message = None):
    bottle.abort(404, message)

  def bad_request(self, text = None):
    bottle.abort(400, text)

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
