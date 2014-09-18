import datetime
import elle
import pymongo
import json
import requests
import time

import infinit.oracles.sisyphus

class Drip(infinit.oracles.sisyphus.Boulder):

  def __init__(self, sisyphus, campaign):
    super().__init__(sisyphus)
    self.__campaign = campaign

  @property
  def campaign(self):
    return self.__campaign

  @property
  def user_fields(self):
    return ['_id',  'email', 'fullname']


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
    # users = meta.users.find_and_modify(
    #   condition,
    #   {
    #     '$set':
    #     {
    #       field: end,
    #     }
    #   },
    #   multi = True,
    #   fields = self.user_fields,
    # )
    # print(condition)
    # users = meta.users.find(
    #   condition,
    #   fields = self.user_fields,
    # )
    users = meta.users.find(
      {'email': {'$in': ['mefyl@gruntech.org',
                         'dimrok@infinit.io',
                         'cadeuh@gmail.com',
                         'bruno.correia@infinit.io']}},
      fields = self.user_fields,
    )
    template = 'drip-%s-%s' % (self.campaign, end.replace('_', '-'))
    sent = self.send_email(end,
                          template,
                          list(users),
                          variations = variations)
    return {'%s -> %s' % (start, end): sent}

  def send_email(self, bucket, template, users,
                 variations = None):
    if variations is not None:
      distribution = self.__distribute(len(users), len(variations))
      res = {}
      for n in range(len(variations)):
        slug = '%s-%s' % (template, variations[n])
        targets = users[distribution[n]:distribution[n + 1]]
        self.send_email(bucket, slug, targets)
        res[slug] = len(targets)
      return res
    else:
      print('%s: send %s to %s users' % (self.campaign, template, len(users)))
      template_content = {}
      message = {
        'to': [
          {
            'email': user['email'],
            'name': user['fullname'],
            'type': 'to',
          }
          for user in users
        ],
        'merge_vars': [
          {
            'rcpt': user['email'],
            'vars': [
              {
                'name': 'USER_%s' % field.capitalize(),
                'content': str(user[field]),
              }
              for field in self.user_fields
            ]
          }
          for user in users
        ],
      }
      res = self.sisyphus.mandrill.messages.send_template(
        template_name = template,
        template_content = template_content,
        message = message,
        async = True,
      )
      print('  mandrill: %s' % res)
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
        for user in users
      ]
      res = requests.post(
        url,
        headers = {'content-type': 'application/json'},
        data = json.dumps({'events': metrics}),
      )
      print('  metrics: %s' % res)


#
#    -> activated   -> reminded
#          ^
#          |----------------------------------
#          |                |                |
#    -> unactivated_1 -> unactivated_2 -> unactivated_3
#

class RegisteredNoTransfer(Drip):

  def __init__(self, sisyphus):
    super().__init__(sisyphus, 'onboarding')
    # Find user in any status without scanning all ghosts, deleted
    # users etc.
    self.sisyphus.mongo.meta.users.ensure_index(
      [
        ('emailing.onboarding.state', pymongo.ASCENDING),
        ('register_status', pymongo.ASCENDING),
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
        # Registered more than three days ago.
        'creation_time':
        {
          '$lt': datetime.datetime.utcnow() - datetime.timedelta(days = 1),
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
          '$lt': datetime.datetime.utcnow() - datetime.timedelta(days = 1),
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
    # # unactivated_1 -> unactivated_2
    # transited = self.transition(
    #   'unactivated_1',
    #   'unactivated_2',
    #   {
    #     # Registered more than 3 days ago.
    #     'creation_time':
    #     {
    #       '$lt': datetime.datetime.utcnow() - datetime.timedelta(days = 3),
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
    #       '$lt': datetime.datetime.utcnow() - datetime.timedelta(days = 7),
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
    #       '$lt': datetime.datetime.utcnow() - datetime.timedelta(days = 7),
    #     },
    #   },
    # )
    # response.update(transited)
    return response
