# -*- encoding: utf-8 -*-

import bottle
import time

import elle.log
from . import conf, mail, error, notifier, transaction_status
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

  @api('/stats')
  def transfer_stats(self):
    res = self.database.transactions.aggregate([
      {'$match': {'status': transaction_status.FINISHED}},
      {'$group': {'_id': 'result',
                  'total_size': {'$sum': '$total_size'},
                  'total_transfers': {'$sum': 1},}},
    ])
    res = res['result']
    if not len(res):
      return self.success({
        'total_size': 0,
        'total_transfers': 0,
        'total_created': 0,
      })
    else:
      res = res[0]
      del res['_id']
      res['total_created'] = self.database.transactions.count()
      return self.success(res)

  @api('/status')
  def status(self):
    return self.success({"status" : True})
    # return self.success(
    #   {
    #     "status" : False,
    #     "message" : "<p>Infinit is under maintainance</p>",
    #   })

  @api('/ghostify', method = 'POST')
  @require_admin
  def ghostify(self, email):
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

  @api('/reset-accounts/<hash>', method = 'GET')
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
    for transaction in self.database.transactions.find(
      {
          "$or": [
            {"sender_id": user['_id']},
            {"recipient_id": user['_id']}
          ]
      },
      fields = ['_id']):
      try:
        self._transaction_update(str(transaction['_id']),
                                 status = transaction_status.CANCELED,
                                 user = user)
      except error.Error as e:
        elle.log.warn("%s" % (e.args,))
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
      avatar = user.get("avatar"),
      email_confirmed = True, # User got a reset account mail, email confirmed.
    )
    return self.success({'user_id': str(user_id)})

  @api('/lost-password', method = 'POST')
  def declare_lost_password(self, email):
    """Generate a reset password url.

    email -- The mail of the infortunate user
    """

    email = email.lower()
    user = self.database.users.find_one({"email": email})
    if not user or user['register_status'] == 'ghost':
      return self.fail(error.UNKNOWN_USER)
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
    self.mailer.send_template(
      to = email,
      template_name = LOST_PASSWORD_TEMPLATE_ID,
      subject = '[Infinit] Reset your password',
      reply_to = 'Infinit <support@infinit.io>',
      merge_vars = {
        email: {
          'reset_password_hash': user['reset_password_hash']
          }}
    )

    return self.success()

  # XXX: Accept 15M body as JSON
  bottle.Request.MEMFILE_MAX = 15 * 1024 * 1024

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
        # Username can contain '@'. If it's not a valid email,
        # the sender address (no-reply@infinit.io) will be used.
        user_email = '@' in user_name and user_name or None
        self.mailer.send(
          to = email,
          reply_to = user_email,
          subject = template['subject'] % {"client_os": client_os},
          body = template['content'] % {
            "client_os": client_os,
            "version": version,
            "user_name": user_name,
            "env": '\n'.join(env),
            "message": message,
            "more": more,
          },
          attachment = ('log.tar.bz', file),
        )
      return self.success()

  @api('/genocide', method = 'POST')
  @require_admin
  def _genocide_(self):
    """
    Make all client commit suicide.
    """
    # XXX: add broadcast capability to trophonius.
    targets = { user['_id'] for user in self.database.users.find({'connected': True}) }
    self.notifier.notify_some(notifier.SUICIDE,
                              message = {},
                              recipient_ids = targets)
    return self.success({'victims': list(targets)})

  @api('/cron', method = 'POST')
  @require_admin
  def cron(self):
    """
    Do cron jobs as:
    - clean old trophonius instances.
    - clean old apertus instances.
    """
    # Trophonius.
    res = self.database.trophonius.remove(
      {"$or": [{"time": {"$lt": time.time() - self.trophonius_expiration_time}},
               {"time": {"$exists": False}}]},
      multi = True)
    # Apertus.
    res = self.database.apertus.remove(
      {"$or": [{"time": {"$lt": time.time() - self.apertus_expiration_time}},
               {"time": {"$exists": False}}]},
      multi = True)

    # import datetime
    # if datetime.datetime.utcnow().hour == self.daily_summary_hour:
    #   self.daily_summary()
    return self.success(res)

  @api('/cron/daily-summary', method = 'POST')
  @require_admin
  def daily_summary(self):
    """
    Send a summary of the unaccepted transfers of the day received after the
    last connection.
    """
    daily_summary_str = 'daily-summary'
    # XXX: Remove exists when it's in prod for a while.
    # It's just to initiate the database.mailer entry.
    exists = self.database.mailer.find_one({'name': daily_summary_str})
    summary = self.database.mailer.find_one(
      {
        'name': daily_summary_str,
        'last-sent': {'$lt': time.time() - 86400 },
      })
    if summary or exists is None:
      with elle.log.trace('run daily cron'):
        # Hardcoded 86400 represents a day in seconds. The system is for daily
        # report.
        query = {
          'status': transaction_status.INITIALIZED,
          'mtime': {'$gt': exists is None and time.time() - 86400 or summary['last-sent']},
        }
        group = {
          '_id': '$recipient_id',
          'mtime': {'$max': '$mtime'},
          'peers': {'$addToSet': '$sender_id'},
          'count': {'$sum': 1},
        }
        transactions = self.database.transactions.aggregate([
          {'$match': query},
          {'$group': group},
          ])['result']

        users = dict()
        for transaction in transactions:
          query = {
            '_id': transaction['_id'],
            'last_connection': {'$lt': transaction['mtime']}
          }
          u = self.database.users.find_one(query, fields = ['email'])
          if u:
            query = {
              '_id': {'$in': transaction['peers']},
            }
            fields = {'fullname': 1, '_id': 0}

            peer = self.database.users.find(query = query,
                                            fields = fields)
            users[u['email']] = {
              'count': transaction['count'],
              'recipients': list(map(lambda x: x['fullname'], peer))
            }

        template_name = 'daily-summary'
        with elle.log.debug('send email'):
          self.mailer.send_template(
            to = list(users.keys()),
            template_name = template_name,
            subject = mail.MAILCHIMP_TEMPLATE_SUBJECTS[template_name],
            merge_vars = users,
          )
        summary = self.database.mailer.find_and_modify(
          {
            'name': daily_summary_str,
          },
          {
            'name': daily_summary_str,
            'last-sent': time.time(),
          }, upsert = True)
        return self.success({"emails": list(users.keys())})
