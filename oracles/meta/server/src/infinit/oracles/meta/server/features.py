ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.User'

import random

from .utils import api, require_admin

class Mixin:

  @api('/features', method = 'POST')
  @require_admin
  def features(self,
               reroll = None,
               abtests = None):
    """ For each feature in abtest, add it to existing users who don't have it.
    """
    keys = set(self.__roll_abtest(False, abtests).keys())
    users = self.database.users.find({}, ['features']) # id is implicit
    for user in users:
      if 'features' not in user:
        user['features'] = {}
      diff = set(keys) - set(user['features'].keys())
      if reroll:
        diff = set(keys)
      if diff:
        features = self.__roll_abtest(False, abtests)
        for k in diff:
          user['features'][k] = features[k]
        self.database.users.update({'_id': user['_id']},
                                   {'$set': { 'features': user['features']}})
    return self.success()

  @api('/features/<name>', method='PUT')
  @require_admin
  def feature_add(self, name, value = None, values = None):
    feature = {'key': name}
    if value is not None:
      feature['value'] = value
    elif values is not None:
      feature['values'] = values
    else:
      self.bad_request('missing value or values parameter')
    self.database.abtest.insert(feature)
    field = 'features.%s' % name
    if value is not None:
      self.database.users.update(
        {},
        {'$set': {field: value}},
        multi = True)
    else:
      for user in self.database.users.find(
          {field: {'$exists': False}}):
        value = self.__roll_feature(values)
        res = self.database.users.update(
          {'_id': user['_id'], field: {'$exists': False}},
          {'$set': {field: value}})
    return feature

  @api('/features/<name>', method='DELETE')
  @require_admin
  def feature_remove(self, name):
    self.database.users.update(
      {},
      {'$unset': {'features.%s' % name: True}})
    self.database.abtest.remove({'key': name})

  def _roll_features(self, from_register, features = {}):
    abtests = self.database.abtest.find()
    for t in abtests:
      if not from_register and t.get('register_only', False):
        continue
      k = t['key']
      if k in features:
        continue
      if 'value' in t:
        v = t['value']
      elif 'values' in t:
        v = self.__roll_feature(t['values'])
      else:
        elle.log.warn('Invalid abtest entry: %s' % t)
        continue
      features[k] = v
    return features

  def __roll_feature(self, values):
    r = random.random()
    accum = 0
    for choice in values:
      accum += values[choice]
      if accum >= r:
        return choice

  @api('/features/<name>')
  @require_admin
  def feature_get(self, name):
    feature = self.database.abtest.find({'key': name})
    if feature is None:
      self.not_found({
        'reason': 'no such feature: %s' % name,
        'feature': name,
      })
    return feature
