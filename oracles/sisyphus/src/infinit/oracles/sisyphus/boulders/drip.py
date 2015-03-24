import bson
import datetime
import elle
import json
import math
import pymongo
import time

from itertools import chain

from .. import Boulder
from .. import version
from infinit.oracles.utils import key
from infinit.oracles.transaction import statuses

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.boulders.drip'


class Emailing(Boulder):

  def __init__(self, sisyphus, campaing, table):
    super().__init__(sisyphus)
    self.__campaign = campaing
    self.__lock = bson.ObjectId()
    self.__table = self.sisyphus.mongo.meta[table]
    self.__table.ensure_index(
      [
        (self.field_lock, pymongo.ASCENDING),
      ],
      sparse = True)

  @property
  def campaign(self):
    return self.__campaign

  @property
  def field_lock(self):
    return 'emailing.%s.lock' % self.campaign

  @property
  def lock_id(self):
    return self.__lock

  @property
  def now(self):
    return datetime.datetime.utcnow()


def avatar(i):
  meta = 'https://meta.api.production.infinit.io'
  return '%s/user/%s/avatar' % (meta, i)

def merge_result(a, b):
  for key, value in b.items():
    if isinstance(value, int):
      a.setdefault(key, 0)
      a[key] += value
    elif isinstance(value, list):
      a.setdefault(key, [])
      a[key] += value
    elif isinstance(value, dict):
      a.setdefault(key, {})
      merge_result(a[key], value)
    else:
      raise Exception('can\'t merge a %s: %s' % (b.__class__, b))

class Drip(Emailing):

  def __init__(self, sisyphus, campaign, table, pretend = False):
    super().__init__(sisyphus, campaign, table)
    self.__table = self._Emailing__table
    self.__pretend = pretend

  @property
  def user_fields(self):
    return [
      '_id',
      'email',
      'fullname',
      'unsubscriptions',
      'features',
      'os',
    ]

  @property
  def fields(self):
    return self.user_fields

  def __distribute(self, n_users, n_variations):
    import random
    res = [n_users // n_variations] * n_variations
    for i in range(n_users % n_variations):
      res[random.randrange(0, n_variations)] += 1
    for i in range(n_variations - 1, -1, -1):
      for j in range(i + 1, n_variations):
        res[j] += res[i]
    return [0] + res

  def transition(self,
                 start,
                 end,
                 condition,
                 template = None,
                 variations = None,
                 update = None):
    meta = self.sisyphus.mongo.meta
    field = 'emailing.%s.state' % self.campaign
    if start is None:
      start_condition = {field: {'$exists': False}}
    elif isinstance(start, str):
      start_condition = {field: start}
    else:
      start_condition = {field: {'$in': start}}
    condition.update(start_condition)
    condition[self.field_lock] = {'$exists': False}
    if template is None:
      template = 'drip_%s_%s' % (self.campaign, end)
    # Uncomment this to go in full test mode.
    # import sys
    # print('%s -> %s: %s (%s)' % (start, end, self.__table.find(condition).count(), template), file = sys.stderr)
    # print(condition, file = sys.stderr)
    # return {}
    final_update = {}
    # if start != end:
    final_update.update({field: end})
    if update is not None:
      final_update.update(update)
    if template is False:
      if self.__pretend:
        n = self.__table.find(condition).count()
      else:
        res = self.__table.update(
          condition,
          {
            '$set': final_update,
          },
          multi = True,
        )
        n = res['n']
      if n > 0:
        return {'%s -> %s' % (start, end): n}
      else:
        return {}
    self.__table.update(
      condition,
      {
        '$set':
        {
          self.field_lock: self.lock_id,
        },
      },
      multi = True,
    )
    result = {}
    elts = list(self.__table.find(
      { self.field_lock: self.lock_id },
      fields = self.fields,
    ))
    if len(elts) > 0:
      users = [(self._user(elt), elt) for elt in elts]
      res = {}
      unsubscribed = [(u, e) for u, e in users if not self.__email_enabled(u)]
      users = [(u, e) for u, e in users if self.__email_enabled(u)]
      templates = self._pick_template(template, users)
      for template, users in templates:
        sent = self.send_email(
          end,
          template,
          users,
          variations = variations)
        res[template] = sent
      update = {
        '$unset':
        {
          self.field_lock: True,
        },
      }
      if not self.__pretend:
        update['$set'] = final_update
      self.__table.update(
        {self.field_lock: self.lock_id},
        update,
        multi = True,
      )
      # Unlock users that were not picked
      unpicked = self.__table.update(
        {self.field_lock: self.lock_id},
        {
          '$unset':
          {
            self.field_lock: True,
          },
        },
        multi = True,
      )
      n = unpicked['n']
      if n > 0:
        res['unpicked'] = n
      if unsubscribed:
        res['unsubscribed'] = [u['email'] for u, e in unsubscribed]
      merge_result(result, res)
    if result == {}:
      return {}
    else:
      return {'%s -> %s' % (start, end): result}

  def _pick_template(self, template, users):
    return [(template, users)]

  def __email_enabled(self, user):
    return 'unsubscriptions' not in user \
      or 'drip' not in user['unsubscriptions']

  def sender(self, v):
    return None

  def send_email(self, bucket, template, users,
                 variations = None):
    with elle.log.trace('%s: send %s to %s users' %
                        (self.campaign, template, len(users))):
      elle.log.dump('users: %s' % (users,))
      if len(users) == 0:
        return []
      if variations is not None:
        distribution = self.__distribute(len(users), len(variations))
        res = {}
        for n in range(len(variations)):
          slug = '%s_%s' % (template, variations[n])
          targets = users[distribution[n]:distribution[n + 1]]
          if len(targets) > 0:
            res[slug] = self.send_email(bucket, slug, targets)
        return res
      else:
        if template is not None:
          recipients = [
            {
              'email': user['email'],
              'name': user['fullname'],
              'vars': dict(chain(
                [
                  ('template', template),
                  ('campaign', self.campaign),
                  ('user', self.user_vars(user)),
                ],
                vars.items(),
              )),
              'sender': self.sender(vars),
            }
            for user, elt, vars in
            ((e[0], e[1], self.__vars(e[1], e[0])) for e in users
             if 'email' in e[0])
          ]
          elle.log.dump('recipients: %s' % (recipients,))
          if self.sisyphus.emailer is not None:
            res = self.sisyphus.emailer.send_template(
              template,
              recipients,
            )
            elle.log.debug('emailer response: %s' % res)
        if self.sisyphus.metrics is not None:
          res = self.sisyphus.metrics.send([
            {
              'event': 'email',
              'campaign': self.campaign,
              'bucket': bucket,
              'template': template,
              'timestamp': time.time(),
              'user': str(user['_id']),
            }
            for user, elt in users
          ])
        return [user['email'] for user, elt in users
                if 'email' in user]

  def _user(self, elt):
    return elt

  def __vars(self, elt, user):
    with elle.log.debug('%s: get variables for %s' % (self, user['email'])):
      return self._vars(elt, user)

  def _vars(self, elt, user):
    return {}

  def user_vars(self, user):
    return {
      'avatar': avatar(user['_id']),
      'email': user['email'],
      'fullname': user['fullname'],
      'id': str(user['_id']),
      'os': user['os'] if 'os' in user else [],
    }

  def transaction_vars(self, transaction, user):
    sender = transaction['sender_id'] == user['_id']
    verb = 'to' if sender else 'from'
    peer = 'recipient' if sender else 'sender'
    return {
      'id': str(transaction['_id']),
      'files': transaction['files'],
      'key': key('/transactions/%s' % transaction['_id']),
      'message': transaction['message'],
      'peer':
      {
        'fullname': transaction['%s_fullname' % peer],
        'id': transaction['%s_id' % peer],
        'avatar': avatar(transaction['%s_id' % peer]),
      },
      'size': transaction['total_size'],
      'verb': verb,
    }

  def status(self):
    res = {}
    locks = self.__table.aggregate(
      [
        {'$match': {self.field_lock: {'$exists': True}}},
        {
          '$group':
          {
            '_id': '$%s' % self.field_lock,
            'count': {'$sum': 1},
          }
        },
      ]
    )['result']
    if locks:
      res['locks'] = [{'id': str(l['_id']), 'count': l['count']}
                      for l in locks]
    return res


#    -> activated
#          ^
#          |----------------------------------
#          |                |                |
#    -> unactivated-1 -> unactivated-2 -> unactivated-3
#

class Onboarding(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'onboarding', 'users', pretend)
    # Find user in any status without scanning all ghosts, deleted
    # users etc.
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.onboarding.state', pymongo.ASCENDING),
        ('register_status', pymongo.ASCENDING),
        ('creation_time', pymongo.ASCENDING),
      ])

  def run(self):
    response = {}
    # -> activated
    transited = self.transition(
      None,
      'activated',
      {
        # Fully registered
        'register_status': 'ok',
        # Registered more than 1 day ago.
        'creation_time':
        {
          '$lt': self.now - self.delay_first_reminder,
        },
        # Did a transaction
        'last_transaction.time': {'$exists': True},
      },
      template = False,
    )
    response.update(transited)
    # -> unactivated-1
    transited = self.transition(
      None,
      'unactivated-1',
      {
        # Fully registered
        'register_status': 'ok',
        # Registered more than 1 day ago.
        'creation_time':
        {
          '$lt': self.now - self.delay_first_reminder,
        },
        # Never did a transaction
        'last_transaction.time': {'$exists': False},
      },
      variations = ('A', 'B'),
    )
    response.update(transited)
    return response

  @property
  def delay_first_reminder(self):
    return datetime.timedelta(days = 3)


class GhostReminder(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus,
                     'ghost-reminder', 'transactions', pretend)
    self.sisyphus.mongo.meta.transactions.ensure_index(
      [
        # Find transactions in any bucket
        ('emailing.ghost-reminder.state', pymongo.ASCENDING),
        # That are ghost
        ('is_ghost', pymongo.ASCENDING),
        # In status ghost uploaded
        ('status', pymongo.ASCENDING),
        # Modified a certain time ago
        ('modification_time', pymongo.ASCENDING),
      ])

  @property
  def now(self):
    return datetime.datetime.utcnow()

  @property
  def delay_first_reminder(self):
    return datetime.timedelta(days = 4)

  def run(self):
    response = {}
    # -> reminded-1
    transited = self.transition(
      None,
      'reminded-1',
      {
        # Fully registered
        'is_ghost': True,
        # Ghost uploaded
        'status': statuses['ghost_uploaded'],
        # Ghost uploaded more than 24h ago
        'modification_time':
        {
          '$lt': self.now - self.delay_first_reminder,
        },
      },
    )
    response.update(transited)
    return response

  @property
  def fields(self):
    return [
      'files',
      'files_count',
      'message',
      'recipient_id',
      'recipient_fullname',
      'sender_fullname',
      'sender_id',
      'total_size',
      'transaction_hash',
    ]

  def _user(self, transaction):
    recipient = transaction['recipient_id']
    return self.sisyphus.mongo.meta.users.find_one(recipient)

  def _vars(self, transaction, recipient):
    sender_id = transaction['sender_id']
    sender = self.sisyphus.mongo.meta.users.find_one(
      sender_id, fields = self.user_fields)
    return {
      'sender': self.user_vars(sender),
      'recipient': self.user_vars(recipient),
      'transaction': self.transaction_vars(transaction, recipient),
    }

  def sender(self, v):
    return ({'fullname': '%s via Infinit' % v['sender']['fullname']})

  def _pick_template(self, template, users):
    return [
      (template, [u for u in users if u[0]['features']['drip_ghost-reminder_template'] == 'a']),
      (None, [u for u in users if u[0]['features']['drip_ghost-reminder_template'] == 'control']),
    ]


#
#    -> 1 -> 2 -> 3
#

class DelightSender(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'delight-sender', 'users', pretend)
    # Find user in any status without scanning all ghosts, deleted
    # users etc.
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.delight-sender.state', pymongo.ASCENDING),
        ('register_status', pymongo.ASCENDING),
        ('transactions.reached', pymongo.ASCENDING),
      ])

  @property
  def now(self):
    return datetime.datetime.utcnow()

  def run(self):
    response = {}
    # -> 1
    transited = self.transition(
      None,
      '1',
      {
        'register_status': 'ok',
        'transactions.reached': {'$gte': self.threshold_first},
      },
    )
    response.update(transited)
    # 1 -> 2
    transited = self.transition(
      '1',
      '2',
      {
        'register_status': 'ok',
        'transactions.reached': {'$gte': self.threshold_second},
      },
    )
    response.update(transited)
    # 2 -> 3
    transited = self.transition(
      '2',
      '3',
      {
        'register_status': 'ok',
        'transactions.reached': {'$gte': self.threshold_third},
      },
    )
    response.update(transited)
    return response

  def _pick_template(self, template, users):
    return [
      (template, [u for u in users if u[0]['features']['drip_delight-sender_template'] == 'a']),
      (None, [u for u in users if u[0]['features']['drip_delight-sender_template'] == 'control']),
    ]

  @property
  def threshold_first(self):
    return 1

  @property
  def threshold_second(self):
    return 5

  @property
  def threshold_third(self):
    return 10

#
# -> 1
#

class DelightRecipient(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'delight-recipient', 'users', pretend)
    # Find user in any status without scanning all ghosts, deleted
    # users etc.
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.delight-recipient.state', pymongo.ASCENDING),
        ('transactions.received_peer', pymongo.ASCENDING),
      ])

  @property
  def now(self):
    return datetime.datetime.utcnow()

  def run(self):
    response = {}
    # -> 1
    transited = self.transition(
      None,
      '1',
      {
        'transactions.received_peer': {'$gte': self.threshold_first},
      },
    )
    response.update(transited)
    return response

  def _pick_template(self, template, users):
    return [
      (template, [u for u in users if u[0]['features']['drip_delight-recipient_template'] == 'a']),
      (None, [u for u in users if u[0]['features']['drip_delight-recipient_template'] == 'control']),
    ]

  @property
  def threshold_first(self):
    return 1

  def _vars(self, elt, user):
    transaction = self.sisyphus.mongo.meta.transactions.find_one(
      {
        'recipient_id': user['_id'],
        'status': {'$in': [statuses['accepted'],
                           statuses['finished']]},
      }
    )
    if transaction is None:
      raise Exception(
        'unable to find received transaction for %s' % user['_id'])
    sender = self.sisyphus.mongo.meta.users.find_one(
      transaction['sender_id'])
    assert sender is not None
    return {
      'sender': self.user_vars(sender),
      'transaction': self.transaction_vars(transaction, user),
    }


#
#    -> 1 -> 2
#

class ConfirmSignup(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'confirm-signup', 'users', pretend)
    # Find user in any status without scanning all ghosts, deleted
    # users etc.
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.confirm-signup.state', pymongo.ASCENDING),
        ('register_status', pymongo.ASCENDING),
        ('email_confirmed', pymongo.ASCENDING),
        ('creation_time', pymongo.ASCENDING),
      ])

  @property
  def now(self):
    return datetime.datetime.utcnow()

  def run(self):
    response = {}
    # -> 1
    transited = self.transition(
      None,
      '1',
      {
        # Fully registered
        'register_status': 'ok',
        # Unconfirmed email
        'email_confirmed': False,
        # Registered more than 3 day ago.
        'creation_time':
        {
          '$lt': self.now - self.delay_first_reminder,
        },
      },
    )
    response.update(transited)
    return response

  @property
  def delay_first_reminder(self):
    return datetime.timedelta(days = 5)

  def _vars(self, elt, user):
    return {
      'confirm_key': key('/users/%s/confirm-email' % user['_id']),
    }

#
# -> 1 -> 2
#

# FIXME: factor with GhostReminder
class WeeklyReport(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'weekly-report', 'users', pretend)
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        # Find initialized users
        ('emailing.weekly-report.state', pymongo.ASCENDING),
        # Fully registered
        ('register_status', pymongo.ASCENDING),
        # Did a transfer
        ('activated', pymongo.ASCENDING),
        # Retention emails are over
        ('emailing.retention.state', pymongo.ASCENDING),
        # With due report
        ('emailing.weekly-report.next', pymongo.ASCENDING),
      ],
      name = 'emailing.weekly-report')
    for role in ('sender', 'recipient'):
      self.sisyphus.mongo.meta.transactions.ensure_index(
        [
          # Find sent transactions
          ('%s_id' % role, pymongo.ASCENDING),
          # Successful
          ('status', pymongo.ASCENDING),
          # In a given period of time
          ('modification_time', pymongo.ASCENDING),
        ])

  def run(self):
    response = {}
    now = self.now
    offset = datetime.timedelta(
      # 3 was for Friday. 0 is for Monday.
      days = now.weekday() + 0,
      hours = now.time().hour - 15,
      minutes = now.time().minute,
      seconds = now.time().second,
      microseconds = now.time().microsecond,
    )
    offset %= datetime.timedelta(weeks = 1)
    self.current = now - offset
    self.previous = self.current - datetime.timedelta(weeks = 1)
    self.next = self.current + datetime.timedelta(weeks = 1)
    elle.log.debug('%s: send report for %s to %s' %
                   (self, self.previous, self.current))
    # -> initialized
    transited = self.transition(
      None,
      'initialized',
      {
        # Fully registered
        'register_status': 'ok',
        # Did a transaction
        'activated': True,
        # Retention is over
        'emailing.retention.state': str(Retention.count),
      },
      template = False,
      update = {
        'emailing.weekly-report.next':
          self.next + datetime.timedelta(weeks = 1),
      },
    )
    response.update(transited)
    # initialized -> initialized
    transited = self.transition(
      'initialized',
      'initialized',
      {
        # Fully registered
        'register_status': 'ok',
        # Did a transaction
        'activated': True,
        # Retention is over
        'emailing.retention.state': str(Retention.count),
        # Report is due
        'emailing.weekly-report.next':
        {
          '$lt': self.now,
        },
      },
      template = 'drip_weekly-report',
      update = {
        'emailing.weekly-report.next': self.next,
      },
    )
    response.update(transited)
    return response

  def _vars(self, element, user):
    res = {}
    peers = list(chain(*(self.sisyphus.mongo.meta.transactions.find(
      {
        '%s_id' % role: user['_id'],
        'status': {'$in': [statuses['finished'],
                           statuses['ghost_uploaded']]},
        'modification_time':
        {
          '$gt': self.previous,
          '$lt': self.current,
        }
      }) for role in ['sender', 'recipient'])))
    res['peer_transactions'] = [
      {
        # 'start': t['creation_time'],
        'files': t['files'],
        'size': t['total_size'],
        'peer':
        {
          'id': str(t['recipient_id']),
          'fullname': t['recipient_fullname'],
          'avatar': avatar(t['recipient_id']),
        }
        if sender else
        {
          'id': str(t['sender_id']),
          'fullname': t['sender_fullname'],
          'avatar': avatar(t['sender_id']),
        },
        'verb': 'to' if sender else 'from',
      } for t, sender in map(
        lambda t: (t, t['sender_id'] == user['_id']),
        sorted(peers,
               key = lambda t: t.get('modification_time'),
               reverse = True))
    ]
    links = list(self.sisyphus.mongo.meta.links.find(
      {
        'sender_id': user['_id'],
        'status': statuses['finished'],
        'mtime':
        {
          '$gt': self.previous,
          '$lt': self.current,
        }
      }
    ))
    res['link_transactions'] = [
      {
        # 'start': t['ctime'],
        'files': [f['name'] for f in t['file_list']],
        'size': sum(f['size'] for f in t['file_list']),
        'count': t['click_count'],
        'url': 'http://inft.ly/%s' % t['hash'],
      } for t in sorted(links,
                        key = lambda t: t.get('click_count'),
                        reverse = True)
    ]
    size = sum(chain(
      (t['total_size'] for t in peers),
      (sum(f['size'] for f in l['file_list']) * l['click_count']
       for l in links)))
    people = len(peers) + sum(l['click_count'] for l in links)
    res.update({
      'people': people,
      'size': size,
      'start': self.previous,
      'end': self.current,
    })
    return res

  def _pick_template(self, template, users):
    return [
      (template, [u for u in users if u[0]['features']['drip_weekly-report_template'] == 'a']),
      (None, [u for u in users if u[0]['features']['drip_weekly-report_template'] == 'control']),
    ]


class ActivityReminder(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'activity-reminder', 'users', pretend)
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        # Find initialized users
        ('emailing.activity-reminder.state', pymongo.ASCENDING),
        # Fully registered
        ('register_status', pymongo.ASCENDING),
        # Disconnected
        ('online', pymongo.ASCENDING),
        # With pending transfers
        ('transactions.activity_has', pymongo.ASCENDING),
        # By disconnection time
        ('emailing.activity-reminder.remind-time', pymongo.ASCENDING),
      ],
      name = 'emailing.activity-reminder')

  def run(self):
    response = {}
    # -> clean
    transited = self.transition(
      None,
      'clean',
      {
        # Fully registered
        'register_status': 'ok',
        # Connected
        'online': True,
      },
      template = False,
    )
    response.update(transited)
    # clean -> stuck
    transited = self.transition(
      'clean',
      'stuck',
      {
        # Fully registered
        'register_status': 'ok',
        # Disconnected
        'online': False,
        # With activity
        'transactions.activity_has': True,
      },
      template = False,
      update = {
        'emailing.activity-reminder.remind-time':
        self.now + self.delay,
      },
    )
    response.update(transited)
    # stuck -> clean when user gets back online
    transited = self.transition(
      'stuck',
      'clean',
      {
        # Fully registered
        'register_status': 'ok',
        # Connected
        'online': True,
      },
      template = False,
    )
    response.update(transited)
    # stuck -> clean if user finished is activity, even if we missed
    # the online transition juste above.
    transited = self.transition(
      'stuck',
      'clean',
      {
        # Fully registered
        'register_status': 'ok',
        # Disconnected
        'online': False,
        # Without activity
        'transactions.activity_has': False,
      },
      template = False,
    )
    response.update(transited)
    # stuck -> stuck
    transited = self.transition(
      'stuck',
      'stuck',
      {
        # Fully registered
        'register_status': 'ok',
        # Disconnected
        'online': False,
        # Has activity
        'transactions.activity_has': True,
        # Reminder is due
        'emailing.activity-reminder.remind-time':
        {
          '$lt': self.now,
        },
      },
      update = {
        'emailing.activity-reminder.remind-time':
        self.now + self.delay,
      },
    )
    response.update(transited)
    return response

  def _vars(self, element, user):
    return {
      'transactions': [
        self.transaction_vars(t, user) for t in
        self.sisyphus.mongo.meta.transactions.find(
          {
            '_id':
            {
              '$in': list(chain(
                user['transactions'].get('pending', []),
                user['transactions'].get('unaccepted', []))),
            }
          })
      ]
    }

  @property
  def delay(self):
    return datetime.timedelta(days = 2)

  @property
  def user_fields(self):
    res = super().user_fields
    res.append('transactions.unaccepted')
    res.append('transactions.pending')
    return res

  def _pick_template(self, template, users):
    return [
      (template, [u for u in users if u[0]['features']['drip_pending-reminder_template'] == 'a']),
      (None, [u for u in users if u[0]['features']['drip_pending-reminder_template'] == 'control']),
    ]


class Retention(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'retention', 'users', pretend)
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.retention.state', pymongo.ASCENDING),
        ('register_status', pymongo.ASCENDING),
        ('activated', pymongo.ASCENDING),
        ('emailing.retention.activation_time', pymongo.ASCENDING),
      ])

  count = 7

  @property
  def now(self):
    return datetime.datetime.utcnow()

  def run(self):
    response = {}
    # -> 1
    transited = self.transition(
      None,
      'activated',
      {
        # Fully registered
        'register_status': 'ok',
        # Activated
        'activated': True,
      },
      template = False,
      update = {
        'emailing.retention.activation_time': self.now,
      }
    )
    response.update(transited)
    for i in range(Retention.count):
      # 1 -> 2
      transited = self.transition(
        str(i) if i > 0 else 'activated',
        str(i + 1),
        {
          # Fully registered
          'register_status': 'ok',
          # Activated
          'activated': True,
          # For long enough
          'emailing.retention.activation_time': {
            '$lt': self.now - self.delay_nth_reminder(i),
          },
        },
      )
      response.update(transited)
    return response

  def delay_nth_reminder(self, nth):
    # Starts after two weeks
    return datetime.timedelta(weeks = nth + 1)
