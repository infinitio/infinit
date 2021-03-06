# -*- encoding: utf-8 -*-

import bottle
import pymongo
import pymongo.read_preferences
import time

from bson.code import Code
from datetime import datetime, timedelta

import elle.log
from . import conf, mail, error, notifier, transaction_status
from .utils import api, require_admin, hash_password
from . import utils
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
    results_valid = timedelta(minutes = 5)
    existing_res = self.database.website_stats.find_one(
      {'timestamp': {'$gt': datetime.utcnow() - results_valid}})
    if existing_res:
      return self.success(existing_res)
    links = self.database.links.aggregate([
      {'$match': {'status': transaction_status.FINISHED}},
      {'$unwind': '$file_list'},
      {
        '$group':
        {
          '_id': '$_id',
          'click_count': {'$first': '$click_count'},
          'size': {'$sum': '$file_list.size'},
        }
      },
      {
        '$project':
        {
          'size': {'$multiply': ['$size', '$click_count']},
        }
      },
      {
        '$group':
        {
          '_id': 'result',
          'count': {'$sum': 1},
          'size': {'$sum': '$size'},
        }
     },
    ])
    links = links['result']
    if links:
      links = links[0]
    else:
      links = {'count': 0, 'size': 0}
    txns = self.database.transactions.aggregate([
      {'$match': {'status': transaction_status.FINISHED}},
      {
        '$group':
        {
          '_id': 'result',
          'size': {'$sum': '$total_size'},
          'count': {'$sum': 1},
        }
      }
    ])
    txns = txns['result']
    if txns:
      txns = txns[0]
    else:
      txns = {'count': 0, 'size': 0}
    res = {
      'total_size': links['size'] + txns['size'],
      'total_count': links['count'] + txns['count'],
      'total_transfers': links['count'] + txns['count'], # backward
      'links_count': links['count'],
      'links_size': links['size'],
      'transfers_count': txns['count'],
      'transfers_size': txns['size'],
      'total_created': self.database.transactions.count() + self.database.transactions.count(),
    }
    # Overwrite existing cached result.
    res['timestamp'] = datetime.utcnow()
    self.database.website_stats.update(
      {'timestamp': {'$lt': res['timestamp']}},
      res,
      upsert = True)
    bottle.response.set_header('Access-Control-Allow-Origin', '*')
    return res

  @api('/status')
  def status(self):
    return self.success({"status" : True})
    # return self.success(
    #   {
    #     "status" : False,
    #     "message" : "<p>Infinit is under maintainance</p>",
    #   })

  @api('/check')
  def check(self):
    table = self.database.check
    try:
      table.update({}, {}, upsert = True)
    except:
      primary = False
    else:
      primary = True
    try:
      pref = pymongo.read_preferences.ReadPreference.NEAREST
      table.count(read_preference = pref)
    except:
      secondary = False
    else:
      secondary = True
    res = {
      'database':
      {
        'primary': primary,
        'secondary': secondary,
      }
    }

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

  # Deprecated
  @api('/lost-password', method = 'POST')
  def declare_lost_password(self,
                            email: utils.enforce_as_email_address):
    """Generate a reset password url.

    email -- The mail of the infortunate user
    """
    return self.lost_password(email)

  # XXX: Accept 15M body as JSON
  bottle.Request.MEMFILE_MAX = 15 * 1024 * 1024

  @api('/debug/report/<type>', method = 'POST')
  def user_report(self,
                  type: str,
                  user_name = 'Unknown',
                  client_os = 'Unknown',
                  message = '',
                  env = [],
                  version = None,
                  email : utils.enforce_as_email_address = 'crash@infinit.io',
                  send = False,
                  more = '',
                  transaction_id = '',
                  file = ''):
    """
    Store the existing crash into database and send a mail if set.
    """
    with elle.log.trace('user report: %s to %s' % (user_name, email)):
      elle.log.trace('to be sent: %s' % type)
      template_dict = {
        'client_os': client_os,
        'version': version,
        "user_name": user_name,
        "env": '\n'.join(env),
        "message": message,
        "more": more,
        "transaction_id": transaction_id
      }
      template = mail.report_templates.get(type, None)
      if template is None:
        self.fail(error.UNKNOWN)
      # Username can contain '@'. If it's not a valid email,
      user_email = '@' in user_name and user_name or None
      if len(file):
        attachment = ('log.tar.bz', file)
      else:
        attachment = None
      subject = template['subject'] % template_dict
      if send:
        res = self.mailer.send(
          to = email,
          fr = user_email,
          subject = subject,
          body = template['content'] % template_dict,
          attachment = attachment,
        )
        for r in res:
          if 'reject_reason' in r and r['reject_reason'] is not None:
            self.bad_request({'reason': r['reject_reason']})
      else:
        print('Would send:\nTO: %s\nRCPT TO: %s\nSUBJECT: %s\nBODY:\n%s\n.\n' %
          (email, user_email, subject, template['content'] % template_dict))
      return {}

  @api('/genocide', method = 'POST')
  @require_admin
  def _genocide_(self):
    """
    Make all client commit suicide.
    """
    # XXX: add broadcast capability to trophonius.
    targets = {
      user['_id']
      for user in self.database.users.find(
          {
            'devices.trophonius': {'$ne': null},
          }
      )}
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
    tropho = self.trophonius_clean();
    # Apertus.
    apertus = self.database.apertus.remove(
      {"$or": [{"time": {"$lt": time.time() - self.apertus_expiration_time}},
               {"time": {"$exists": False}}]},
      multi = True)['n']
    # import datetime
    # if datetime.datetime.utcnow().hour == self.daily_summary_hour:
    #   self.daily_summary()
    return {
      'trophonius': tropho,
      'apertus': apertus,
    }

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
