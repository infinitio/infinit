import bson
import datetime
import elle
import pymongo
import json
import requests
import time

from itertools import chain

from .. import Boulder
from .. import version
from infinit.oracles.utils import key
from infinit.oracles.transaction import statuses

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.boulders.drip'

class Drip(Boulder):

  def __init__(self, sisyphus, campaign, table):
    super().__init__(sisyphus)
    self.__campaign = campaign
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
  def user_fields(self):
    return ['_id',  'email', 'fullname', 'unsubscriptions', 'features']

  @property
  def fields(self):
    return self.user_fields

  @property
  def lock_id(self):
    return self.__lock

  @property
  def field_lock(self):
    return 'emailing.%s.lock' % self.campaign

  def __distribute(self, n_users, n_variations):
    import random
    res = [n_users // n_variations] * n_variations
    for i in range(n_users % n_variations):
      res[random.randrange(0, n_variations)] += 1
    for i in range(n_variations - 1, -1, -1):
      for j in range(i + 1, n_variations):
        res[j] += res[i]
    return [0] + res

  def transition(self, start, end, condition,
                 template = None, variations = None):
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
      template = 'drip-%s-%s' % (self.campaign, end.replace('_', '-'))
    # Uncomment this to go in full test mode.
    # import sys
    # print('%s -> %s: %s (%s)' % (start, end, self.__table.find(condition).count(), template), file = sys.stderr)
    # print(condition, file = sys.stderr)
    # return {}
    if template is False:
      res = self.__table.update(
        condition,
        {
          '$set':
          {
            field: end,
          },
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
        self.__table.update(
          {self.field_lock: self.lock_id},
          {
            # TEST MODE 2: comment the $set out
            '$set':
            {
              field: end,
            },
            '$unset':
            {
              self.field_lock: True,
            },
          },
          multi = True,
        )
        res[template] = sent
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
      return {'%s -> %s' % (start, end): res}
    else:
      return {}

  def _pick_template(self, template, users):
    return [(template, users)]

  def __email_enabled(self, user):
    return 'unsubscriptions' not in user \
      or 'drip' not in user['unsubscriptions']

  def send_email(self, bucket, template, users,
                 variations = None):
    with elle.log.trace('%s: send %s to %s users' %
                        (self.campaign, template, len(users))):
      if len(users) == 0:
        return []
      if variations is not None:
        distribution = self.__distribute(len(users), len(variations))
        res = {}
        for n in range(len(variations)):
          slug = '%s-%s' % (template, variations[n])
          targets = users[distribution[n]:distribution[n + 1]]
          if len(targets) > 0:
            res[slug] = self.send_email(bucket, slug, targets)
        return res
      else:
        if template is not None:
          recipients = [
            {
              # TEST MODE 2
              # 'email': 'gaetan@infinit.io',
              'email': user['email'],
              'name': user['fullname'],
              'vars': dict(chain(
                [
                  ('TEMPLATE', template),
                  ('INFINIT_SCHEME', 'infinit://'),
                ],
                self.user_vars('USER', user).items(),
                self._vars(elt, user).items(),
              ))
            }
            for user, elt in users
          ]
          res = self.sisyphus.emailer.send_template(
            template,
            recipients,
          )
          elle.log.debug('mandrill answer: %s' % res)
        url = 'http://metrics.9.0.api.production.infinit.io/collections/users'
        metrics = [
          {
            'event': 'email',
            'campaign': self.campaign,
            'bucket': bucket,
            'template': template,
            'timestamp': time.time(),
            'user': str(user['_id']),
          }
          for user, elt in users
        ]
        res = requests.post(
          url,
          headers = {'content-type': 'application/json'},
          data = json.dumps({'events': metrics}),
        )
        elle.log.debug('metrics answer: %s' % res)
        return [user['email'] for user, elt in users]

  def _user(self, elt):
    return elt

  def _vars(self, elt, user):
    return {}

  def user_vars(self, name, user):
    name = name.upper()
    meta = 'https://meta.api.production.infinit.io'
    avatar = '%s/user/%s/avatar' % (meta, user['_id'])
    return {
      '%s_AVATAR' % name: avatar,
      '%s_EMAIL' % name: user['email'],
      '%s_FULLNAME' % name: user['fullname'],
      '%s_ID' % name: str(user['_id']),
    }

  def transaction_vars(self, name, transaction):
    name = name.upper()
    return {
      '%s_ID' % name: str(transaction['_id']),
      '%s_FILENAME' % name: transaction['files'][0],
      '%s_FILES_COUNT' % name: transaction['files_count'],
      '%s_FILES_COUNT_OTHER' % name: transaction['files_count'] - 1,
      '%s_KEY' % name: key('/transactions/%s' % transaction['_id']),
      '%s_MESSAGE' % name: transaction['message'],
    }


#    -> activated
#          ^
#          |----------------------------------
#          |                |                |
#    -> unactivated-1 -> unactivated-2 -> unactivated-3
#

class Onboarding(Drip):

  def __init__(self, sisyphus):
    super().__init__(sisyphus, 'onboarding', 'users')
    # Find user in any status without scanning all ghosts, deleted
    # users etc.
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.onboarding.state', pymongo.ASCENDING),
        ('register_status', pymongo.ASCENDING),
        ('creation_time', pymongo.ASCENDING),
      ])

  @property
  def now(self):
    return datetime.datetime.utcnow()

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
    )
    response.update(transited)
    # unactivated-1 -> activated
    transited = self.transition(
      'unactivated-1',
      'activated',
      {
        # Fully registered
        'register_status': 'ok',
        # Registered more than 2 days ago.
        'creation_time':
        {
          '$lt': self.now - self.delay_second_reminder,
        },
        # Did a transaction
        'last_transaction.time': {'$exists': True},
      },
      template = False,
     )
    response.update(transited)
    # unactivated-1 -> unactivated-2
    transited = self.transition(
      'unactivated-1',
      'unactivated-2',
      {
        # Fully registered
        'register_status': 'ok',
        # Registered more than 2 days ago.
        'creation_time':
        {
          '$lt': self.now - self.delay_second_reminder,
        },
        # Never did a transaction
        'last_transaction.time': {'$exists': False},
      },
    )
    response.update(transited)
    # unactivated-2 -> activated
    transited = self.transition(
      'unactivated-2',
      'activated',
      {
        # Fully registered
        'register_status': 'ok',
        # Registered more than 2 days ago.
        'creation_time':
        {
          '$lt': self.now - self.delay_third_reminder,
        },
        # Did a transaction
        'last_transaction.time': {'$exists': True},
      },
      template = False,
     )
    response.update(transited)
    # unactivated-2 -> unactivated-3
    transited = self.transition(
      'unactivated-2',
      'unactivated-3',
      {
        # Fully registered
        'register_status': 'ok',
        # Registered more than 3 days ago.
        'creation_time':
        {
          '$lt': self.now - self.delay_third_reminder,
        },
        # Never did a transaction
        'last_transaction.time': {'$exists': False},
      },
    )
    response.update(transited)
    return response

  def _pick_template(self, template, users):
    return [
      (template, [u for u in users if u[0]['features']['drip_onboarding_template'] == 'a']),
      (None, [u for u in users if u[0]['features']['drip_onboarding_template'] == 'control']),
    ]

  @property
  def delay_first_reminder(self):
    return datetime.timedelta(days = 1)

  @property
  def delay_second_reminder(self):
    return datetime.timedelta(days = 2)

  @property
  def delay_third_reminder(self):
    return datetime.timedelta(days = 3)


class GhostReminder(Drip):

  def __init__(self, sisyphus):
    super().__init__(sisyphus, 'ghost-reminder', 'transactions')
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
    return datetime.timedelta(days = 1)

  @property
  def delay_second_reminder(self):
    return datetime.timedelta(days = 3)

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
    # reminded-1 -> reminded-2
    transited = self.transition(
      'reminded-1',
      'reminded-2',
      {
        # Fully registered
        'is_ghost': True,
        # Ghost uploaded
        'status': statuses['ghost_uploaded'],
        # Ghost uploaded more than 24h ago
        'modification_time':
        {
          '$lt': self.now - self.delay_second_reminder,
        },
      },
    )
    response.update(transited)
    return response

  @property
  def fields(self):
    return ['recipient_id', 'sender_id',
            'files', 'message', 'files_count', 'transaction_hash']

  def _user(self, transaction):
    recipient = transaction['recipient_id']
    return self.sisyphus.mongo.meta.users.find_one(recipient)

  def _vars(self, transaction, recipient):
    sender_id = transaction['sender_id']
    sender = self.sisyphus.mongo.meta.users.find_one(
      sender_id, fields = self.user_fields)
    res = {}
    res.update(self.user_vars('sender', sender))
    res.update(self.user_vars('recipient', recipient))
    res.update(self.transaction_vars('transaction', transaction))
    return res

  def _pick_template(self, template, users):
    return [
      (template, [u for u in users if u[0]['features']['drip_ghost-reminder_template'] == 'a']),
      (None, [u for u in users if u[0]['features']['drip_ghost-reminder_template'] == 'control']),
    ]


#
#    -> 1 -> 2 -> 3
#

class DelightSender(Drip):

  def __init__(self, sisyphus):
    super().__init__(sisyphus, 'delight-sender', 'users')
    # Find user in any status without scanning all ghosts, deleted
    # users etc.
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.delight-sender.state', pymongo.ASCENDING),
        ('transactions.sent', pymongo.ASCENDING),
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
        'transactions.sent': {'$gte': self.threshold_first},
      },
    )
    response.update(transited)
    # 1 -> 2
    transited = self.transition(
      '1',
      '2',
      {
        'transactions.sent': {'$gte': self.threshold_second},
      },
    )
    response.update(transited)
    # 2 -> 3
    transited = self.transition(
      '2',
      '3',
      {
        'transactions.sent': {'$gte': self.threshold_third},
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

  def __init__(self, sisyphus):
    super().__init__(sisyphus, 'delight-recipient', 'users')
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
    assert transaction is not None
    sender = self.sisyphus.mongo.meta.users.find_one(
      transaction['sender_id'])
    assert sender is not None
    return self.user_vars('sender', sender)
