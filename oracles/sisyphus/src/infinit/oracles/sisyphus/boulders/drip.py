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
import infinit.oracles.emailer

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.boulders.drip'

meta = 'https://meta.api.production.infinit.io'

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

  def __init__(self,
               sisyphus,
               campaign,
               table,
               pretend = False,
               list = None):
    super().__init__(sisyphus, campaign, table)
    self.__list = list
    self.__pretend = pretend
    self.__table = self._Emailing__table

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
    final_update = {field: end}
    if update is not None:
      final_update.update(update)
    if template is None:
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
    else:
      # Lock documents
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
      # Fetch documents
      elts = self.__table.find({self.field_lock: self.lock_id},
                               fields = self.fields)
      # Split unsubscribed
      users = []
      unsubscribed = []
      for u, e in [(self._user(elt), elt) for elt in elts]:
        (users if self.__email_enabled(u) else unsubscribed)\
          .append((u, e))
      # Send email
      sent = self.send_email(
        end,
        template,
        users,
        variations = variations)
      update = {
        '$unset':
        {
          self.field_lock: True,
        },
      }
      # Unlock and commit
      if not self.__pretend:
        update['$set'] = final_update
      self.__table.update(
        {self.field_lock: self.lock_id},
        update,
        multi = True,
      )
      # Format result
      res = {}
      if len(sent) > 0:
        res[template] = sent
      if len(unsubscribed) > 0:
        res['unsubscribed'] = [u['email'] for u, e in unsubscribed]
      if res == {}:
        return {}
      else:
        return {'%s -> %s' % (start, end): res}

  def _pick_template(self, template, users):
    return [(template, users)]

  def __email_enabled(self, user):
    if self.__list is not None:
      return 'unsubscriptions' not in user \
        or self.__list not in user['unsubscriptions']
    else:
      return True

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
                  ('list', self.__list),
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
          ],
          collection = 'users')
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
    return infinit.oracles.emailer.user_vars(user, meta)

  def transaction_vars(self, transaction, user):
    return infinit.oracles.emailer.transaction_vars(
      transaction, user, meta)

  def avatar(self, user_id):
    return infinit.oracles.emailer.avatar(user_id, meta)

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
    super().__init__(sisyphus, 'onboarding', 'users',
                     pretend, list = 'tips')
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
          '$lt': self.now - self.delay,
        },
        # Did a transaction
        'last_transaction.time': {'$exists': True},
      },
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
          '$lt': self.now - self.delay,
        },
        # Never did a transaction
        'last_transaction.time': {'$exists': False},
      },
      template = 'Unactivated',
    )
    response.update(transited)
    return response

  @property
  def delay(self):
    return datetime.timedelta(days = 5)


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
  def delay(self):
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
        # Uploaded more than 4d ago
        'modification_time':
        {
          '$lt': self.now - self.delay,
        },
      },
      template = 'Transfer (Reminder)',
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
    return {
      'fullname': '%s via Infinit' % v['sender']['fullname'],
    }


#    -> 1

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
        # Registered more than 5 day ago.
        'creation_time':
        {
          '$lt': self.now - self.delay,
        },
      },
      template = 'Confirm Registration (Reminder)',
    )
    response.update(transited)
    return response

  @property
  def delay(self):
    return datetime.timedelta(days = 5)

  def _vars(self, elt, user):
    return {
      'confirm_key': key('/users/%s/confirm-email' % user['_id']),
    }


class ActivityReminder(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'activity-reminder', 'users',
                     pretend, list = 'alerts')
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
    # Stuck if offline with activity
    for start in (None, 'clean'):
      transited = self.transition(
        start,
        'stuck-0',
        {
          # Fully registered
          'register_status': 'ok',
          # Disconnected
          'online': False,
          # With activity
          'transactions.activity_has': True,
        },
        update = {
          'emailing.activity-reminder.remind-time':
          self.now + self.delay(0),
        },
      )
      response.update(transited)
    # Back to clean if online
    for start in chain((None,), ('stuck-%s' % i for i in range(5))):
      transited = self.transition(
        start,
        'clean',
        {
          # Fully registered
          'register_status': 'ok',
          # Connected
          'online': True,
        },
      )
      response.update(transited)
    for stuckiness in range(5):
      # Back to clean if user finished his activity, even if we missed
      # the online transition.
      transited = self.transition(
        'stuck-%s' % stuckiness,
        'clean',
        {
          # Fully registered
          'register_status': 'ok',
          # Disconnected
          'online': False,
          # Without activity
          'transactions.activity_has': False,
        },
      )
      response.update(transited)
    for stuckiness in range(5):
      if stuckiness == 4:
        update = {
          'emailing.activity-reminder.remind-time': None,
        }
      else:
        update = {
          'emailing.activity-reminder.remind-time':
          self.now + self.delay(stuckiness + 1),
        }
      # stuck -> stuck
      transited = self.transition(
        'stuck-%s' % stuckiness,
        'stuck-%s' % (stuckiness + 1),
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
        template = 'Pending',
        update = update,
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

  def delay(self, n):
    if n == 0:
      return datetime.timedelta(hours = 8)
    elif n == 1:
      return datetime.timedelta(days = 2)
    elif n == 2:
      return datetime.timedelta(weeks = 1)
    elif n == 3:
      return datetime.timedelta(weeks = 2)
    elif n == 4:
      return datetime.timedelta(weeks = 4)
    else:
      raise IndexError()

  @property
  def user_fields(self):
    res = super().user_fields
    res.append('transactions.unaccepted')
    res.append('transactions.pending')
    return res


class Tips(Drip):

  def __init__(self, sisyphus, pretend = False):
    super().__init__(sisyphus, 'tips', 'users', pretend)
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.tips.state', pymongo.ASCENDING),
        ('register_status', pymongo.ASCENDING),
      ])

  @property
  def now(self):
    return datetime.datetime.utcnow()

  @property
  def delay(self):
    return datetime.timedelta(weeks = 2)

  def run(self):
    response = {}
    # -> 1
    transited = self.transition(
      None,
      'fresh',
      {
        # Fully registered
        'register_status': 'ok',
      },
      update = {
        'emailing.tips.next': self.now + self.delay,
      }
    )
    response.update(transited)
    transited = self.transition(
      'fresh',
      'send-to-self',
      {
        # Fully registered
        'register_status': 'ok',
        # For long enough
        'emailing.tips.next': {
          '$lt': self.now,
        },
      },
    )
    response.update(transited)
    return response
