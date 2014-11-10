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
    return ['_id',  'email', 'fullname', 'unsubscriptions']

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

  def transition(self, start, end, condition, variations = None):
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
    # Uncomment this to go in full test mode.
    # import sys
    # print('%s -> %s: %s' % (start, end, meta.users.find(condition).count()), file = sys.stderr)
    # print(condition, file = sys.stderr)
    # return {}
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
    template = 'drip-%s-%s' % (self.campaign, end.replace('_', '-'))
    if len(elts) > 0:
      sent = self.send_email(
        end,
        template,
        [(self._user(elt), elt) for elt in elts],
        variations = variations)
      self.__table.update(
        {self.field_lock: self.lock_id},
        {
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
      return {'%s -> %s' % (start, end): sent}
    else:
      return {}

  def unsubscribe_link(self, user, bucket, template):
    user_id = user['_id']
    k = key('/users/%s/email_subscriptions/drip' % user_id)
    url = 'http://infinit.io/unsubscribe/drip'
    return '%s?email=%s&key=%s&utm_source=drip&utm_campaign=%s&bucket=%s&utm_content=%s' % \
      (url, user_id, k, self.campaign, bucket, template)

  def __email_enabled(self, user):
    return 'unsubscriptions' not in user \
      or 'drip' not in user['unsubscriptions']

  def send_email(self, bucket, template, users,
                 variations = None):
    with elle.log.trace('%s: send %s to %s users' %
                        (self.campaign, template, len(users))):
      if len(users) == 0:
        return 0
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
        print()
        template_content = {}
        message = {
          'to': [
            {
              'email': user['email'],
              'name': user['fullname'],
              'type': 'to',
            }
            for user, elt in users if self.__email_enabled(user)
          ],
          'merge_vars': [
            {
              'rcpt': user['email'],
              'vars': [
                {
                  'name': 'USER_%s' % field.upper(),
                  'content': str(user[field]),
                }
                for field in self.user_fields if field in user
              ] + [
                {
                  'name': 'UNSUB',
                  'content': self.unsubscribe_link(user, bucket, template),
                }
              ] + [
                {
                  'name': key,
                  'content': value,
                } for key, value in self._vars(elt, user).items()

              ]
            }
            for user, elt in users if self.__email_enabled(user)
          ],
        }
        res = self.sisyphus.mandrill.messages.send_template(
          template_name = template,
          template_content = template_content,
          message = message,
          async = True,
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
          for user, elt in users if self.__email_enabled(user)
        ]
        res = requests.post(
          url,
          headers = {'content-type': 'application/json'},
          data = json.dumps({'events': metrics}),
        )
        elle.log.debug('metrics answer: %s' % res)
        return len(users)

  def _user(self, elt):
    return elt

  def _vars(self, elt, user):
    return {}

  def user_vars(self, name, user):
    name = name.upper()
    meta = 'https://meta.%s.%s.api.production.infinit.io' \
           % (version.minor, version.major)
    avatar = '%s/user/%s/avatar' % (meta, user['_id'])
    return {
      '%s_AVATAR' % name: avatar,
      '%s_EMAIL' % name: user['email'],
      '%s_FULLNAME' % name: user['fullname'],
      '%s_ID' % name: str(user['_id']),
    }

  def transaction_vars(self, name, transaction):
    name = name.upper()
    print(transaction)
    return {
      '%s_ID' % name: str(transaction['_id']),
      '%s_FILENAME' % name: transaction['files'][0],
      '%s_FILES_COUNT' % name: transaction['files_count'],
      '%s_FILES_COUNT_OTHER' % name: transaction['files_count'] - 1,
      '%s_KEY' % name: key('/transaction/%s' % transaction['_id']),
      '%s_MESSAGE' % name: transaction['message'],
    }


#
#    -> activated   -> reminded
#          ^
#          |----------------------------------
#          |                |                |
#    -> unactivated_1 -> unactivated_2 -> unactivated_3
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
        # Registered more than a day ago.
        'creation_time':
        {
          '$lt': self.now - datetime.timedelta(days = 1),
        },
        # Did a transaction
        'last_transaction.time': {'$exists': True},
      },
      variations = ('var1', 'var2'),
    )
    response.update(transited)
    # -> unactivated_1
    transited = self.transition(
      None,
      'unactivated_1',
      {
        # Fully registered
        'register_status': 'ok',
        # Registered more than a day ago.
        'creation_time':
        {
          '$lt': self.now - datetime.timedelta(days = 1),
        },
        # Never did a transaction
        'last_transaction.time': {'$exists': False},
      },
      variations = ('var1', 'var2'),
    )
    response.update(transited)
    # unactivated_{1,2,3} -> activated
    transited = self.transition(
      ['unactivated_1', 'unactivated_2', 'unactivated_3'],
      'activated',
      {
        # Did a transaction
        'last_transaction.time': {'$exists': True},
      },
      variations = ('var1', 'var2'),
    )
    response.update(transited)
    # unactivated_1 -> unactivated_2
    transited = self.transition(
      'unactivated_1',
      'unactivated_2',
      {
        # Fully registered
        'register_status': 'ok',
        # Registered more than 3 days ago.
        'creation_time':
        {
          '$lt': self.now - datetime.timedelta(days = 4),
        },
        # Never did a transaction
        'last_transaction.time': {'$exists': False},
      },
      variations = ('var1', 'var2'),
    )
    response.update(transited)
    # # unactivated_1 -> unactivated_2
    # transited = self.transition(
    #   'unactivated_1',
    #   'unactivated_2',
    #   {
    #     # Registered more than 3 days ago.
    #     'creation_time':
    #     {
    #       '$lt': self.now - datetime.timedelta(days = 3),
    #     },
    #     # Never did a transaction
    #     'last_transaction.time': {'$exists': True},
    #   },
    # )
    # response.update(transited)
    # # unactivated_2 -> unactivated_3
    # transited = self.transition(
    #   'unactivated_2',
    #   'unactivated_3',
    #   {
    #     # Registered more than 7 days ago.
    #     'creation_time':
    #     {
    #       '$lt': self.now - datetime.timedelta(days = 7),
    #     },
    #     # Never did a transaction
    #     'last_transaction.time': {'$exists': True},
    #   },
    # )
    # response.update(transited)
    # # {,re}activated -> reactivated
    # transited = self.transition(
    #   ['activated', 'reactivated'],
    #   'reactivated',
    #   {
    #     # Did a transaction
    #     'last_transaction.time':
    #     {
    #       '$lt': self.now - datetime.timedelta(days = 7),
    #     },
    #   },
    # )
    # response.update(transited)
    return response


class GhostReminder(Drip):

  def __init__(self, sisyphus):
    super().__init__(sisyphus, 'ghost_reminder', 'transactions')
    self.sisyphus.mongo.meta.transactions.ensure_index(
      [
        # Find transactions in any bucket
        ('emailing.ghost_reminder.state', pymongo.ASCENDING),
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
        'status': 4,
        # Ghost uploaded more than 24h ago
        'modification_time':
        {
          '$lt': self.now - self.delay_first_reminder,
        },
      },
    )
    # -> reminded-2
    transited = self.transition(
      'reminded-1',
      'reminded-2',
      {
        # Fully registered
        'is_ghost': True,
        # Ghost uploaded
        'status': 4,
        # Ghost uploaded more than 24h ago
        'modification_time':
        {
          '$lt': self.now - self.delay_second_reminder,
        },
      },
    )

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
