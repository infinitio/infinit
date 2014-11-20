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

  @api('/features/<name>', method='DELETE')
  @require_admin
  def feature_remove(self, name):
    users = self.database.users.find({}, ['features']) # id is implicit
    for user in users:
      if not 'features' in user:
        continue
      if name in user['features']:
        del user['features'][name]
        self.database.users.update({'_id': user['_id']},
                                   {'$set': { 'features': user['features']}})
    self.database.abtest.remove({'key': name})

  @api('/features/<name>', method='POST')
  @require_admin
  def feature_add(self, name, value = None, values = None):
    feature = {'key': name}
    if value is not None:
      feature['value'] = value
    elif values is not None:
      feature['values'] = values
    else:
      raise error.ERROR(
        error.OPERATION_NOT_PERMITTED,
        "Missing value or values field.")
    self.database.abtest.insert(feature)

  def _roll_features(self, from_register):
    features = {}
    abtests = self.database.abtest.find({})
    for t in abtests:
      if not from_register and t.get('register_only', False):
        continue
      k = t['key']
      if 'value' in t:
        v = t['value']
      elif 'values' in t:
        r = random.random()
        accum = 0
        for choice in t['values']:
          accum += t['values'][choice]
          if accum >= r:
            v = choice
            break
      else:
        elle.log.warn('Invalid abtest entry: %s' % t)
        continue
      features[k] = v
    return features
