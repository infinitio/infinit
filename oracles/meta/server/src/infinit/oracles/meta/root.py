# -*- encoding: utf-8 -*-

import time

import elle.log
from . import conf, mail, error, notifier
from .utils import api, require_admin, hash_pasword
import infinit.oracles.meta.version

LOST_PASSWORD_TEMPLATE_ID = 'lost-password'
RESET_PASSWORD_VALIDITY = 2 * 3600 # 2 hours

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Root'

class Mixin:

  @api('/')
  def root(self):
    return self.success({
        'server': 'Meta %s' % infinit.oracles.meta.version.version,
        'logged_in': self.user is not None,
        # 'fallbacks': str(self.__application__.fallback),
    })

  @api('/status')
  def status(self):
    return self.success({"status" : "ok"})

  @api('/ghostify', method = 'POST')
  @require_admin
  def ghostify(self, email, admin_token):
    email.strip()

    user = self.database.users.find_one({"email": email})

    if user is None:
      return self.error(error.UNKNOWN_USER)

    # Invalidate all transactions.
    # XXX: Peers should be notified.
    from meta.resources import transaction
    self.database.transactions.update(
      {"$or": [{"sender_id": user['_id']}, {"recipient_id": user['_id']}]},
      {"$set": {"status": transaction.CANCELED}}, multi=True)

    keys = ['_id', 'email', 'fullname', 'ghost', 'swaggers', 'accounts',
            'remaining_invitations', 'handle', 'lw_handle']

    ghost = dict()
    for key in keys:
      value = user.get(key)
      if value is not None:
        ghost[key] = value

    # Ghostify user.
    ghost = self.registerUser(**ghost)

    invitation.invite_user(user['email'], database = self.database)

    return self.success({'ghost': str(user['_id'])})

  def __user_from_hash(self, hash):
    user = self.database.users.find_one({"reset_password_hash": hash})
    if user is None:
      raise error.Error(
        error.OPERATION_NOT_PERMITTED,
        "Your password has already been reset",
      )
    if user['reset_password_hash_validity'] < time.time():
      raise error.Error(
        error.OPERATION_NOT_PERMITTED,
        "The reset url is not valid anymore",
      )
    return user

  @api('/reset-accounts/<hash>')
  def reseted_account(self, hash):
    """Reset account using the hash generated from the /lost-password page.

    hash -- the reset password token.
    """
    with elle.log.trace('reseted account %s' % hash):
      try:
        user = self.__user_from_hash(hash)
        elle.log.debug('found user %s' % user['email'])
      except error.Error as e:
        self.fail(*e.args)
      return self.success(
        {
          'email': user['email'],
        }
      )

  @api('/reset-accounts/<hash>', method = 'POST')
  def reset_account(self, hash, password):
    try:
      user = self.__user_from_hash(hash)
    except error.Error as e:
      self.fail(*e.args)

    # Cancel all the current transactions.
    for transaction_id in self.database.transactions.find(
      {
          "$or": [
            {"sender_id": user['_id']},
            {"recipient_id": user['_id']}
          ]
        },
        fields = ['_id']):
      try:
        self._transaction_update(transaction_id, transaction.CANCELED, user)
      except error.Error as e:
        elle.log.warn("%s" % e.args)
        continue
        # self.fail(error.UNKNOWN)
    # Remove all the devices from the user because they are based on his old
    # public key.
    # XXX: All the sessions must be cleaned too.
    # XXX: Must be handle by the client.
    self.database.devices.remove({"owner": user['_id']},
                                 multi = True)

    import papier
    identity, public_key = papier.generate_identity(
      str(user["_id"]),
      user['email'],
      password,
      conf.INFINIT_AUTHORITY_PATH,
      conf.INFINIT_AUTHORITY_PASSWORD
    )

    user_id = self._register(
      _id = user["_id"],
      register_status = 'ok',
      email = user['email'],
      fullname = user['fullname'],
      password = hash_pasword(password),
      identity = identity,
      public_key = public_key,
      handle = user['handle'],
      lw_handle = user['lw_handle'],
      swaggers = user['swaggers'],
      networks = [],
      devices = [],
      connected_devices = [],
      connected = False,
      notifications = [],
      old_notifications = [],
      accounts = [
        {'type': 'email', 'id': user['email']}
      ],
    )
    return self.success({'user_id': str(user_id)})

  @api('/lost-password', method = 'POST')
  def declare_lost_password(self, email):
    """Generate a reset password url.

    email -- The mail of the infortunate user
    """

    email = email.lower()
    user = self.database.users.find_one({"email": email})
    if not user:
      return self.error(error_code = error.UNKNOWN_USER)
    import time, hashlib
    hash = str(time.time()) + email
    hash = hash.encode('utf-8')
    self.database.users.update(
      {'email': email},
      {'$set':
        {
          'reset_password_hash': hashlib.md5(hash).hexdigest(),
          'reset_password_hash_validity': time.time() + RESET_PASSWORD_VALIDITY,
        }
      })

    user = self.database.users.find_one({'email': email}, fields = ['reset_password_hash'])
    self.mailer.templated_send(
      to = email,
      template_id = LOST_PASSWORD_TEMPLATE_ID,
      subject = '[Infinit] Reset your password',
      reply_to = 'Infinit <support@infinit.io>',
      reset_password_hash = user['reset_password_hash'],
    )

    return self.success()

  @api('/debug/report/<type>', method = 'POST')
  def user_report(self,
                  type: str,
                  user_name = 'Unknown',
                  client_os = 'Unknown',
                  message = [],
                  env = [],
                  version = 'Unknown version',
                  email = 'crash@infinit.io',
                  send = False,
                  more = '',
                  file = ''):
    """
    Store the existing crash into database and send a mail if set.
    """
    with elle.log.trace('user report: %s to %s' % (user_name, email)):
      if send:
        elle.log.trace('to be sent: %s' % type)
        template = mail.report_templates.get(type, None)
        if template is None:
          self.fail(error.UNKNOWN)
        self.mailer.send(
          to = email,
          subject = template['subject'] % {"client_os": client_os},
          content = template['content'] % {
            "client_os": client_os,
            "version": version,
            "user_name": user_name,
            "env": '\n'.join(env),
            "message": message,
            "more": more,
          },
          attached = ('log.tar.gz', file),
        )
      return self.success()

  @require_admin
  @api('/genocide', method = 'POST')
  def _genocide_(self, admin_token):
    """
    Make all client commit suicide.
    """
    # XXX: add broadcast capability to trophonius.
    targets = { user['_id'] for user in self.database.users.find({'connected': True}) }
    self.notifier.notify_some(notifier.SUICIDE,
                              message = {},
                              recipient_ids = targets)
    return self.success({'victims': list(targets)})

  @require_admin
  @api('/cron', method = 'POST')
  def cron(self, admin_token):
    """
    Do cron jobs as:
    - clean old trophonius instances.
    """
    # Trophonius.
    res = self.database.trophonius.remove(
      {"$or": [{"time": {"$lt": time.time() - self.trophonius_expiration_time}},
               {"time": {"$exists": False}}]},
      multi = True)
    return self.success(res)
