# -*- encoding: utf-8 -*-

import elle.log
from elle.log import log, trace, debug, dump
from . import utils
from .utils import *
import stripe

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Plan'

class Plan(dict):

  # ----------- #
  # Properties. #
  # ----------- #
  @property
  def name(self):
    """Return the plan name.
    """
    return self.get('name', self['stripe']['name'])

  @property
  def links_quota(self):
    """Return the associated plan link quota.
    All plans *must* have quotas.links.quota.
    """
    return int(self['quotas']['links']['quota'])

  # ------------- #
  # Construction. #
  # ------------- #
  def __init__(
      self,
      meta,
      body,
      stripe_info,
      team = True):
    elle.log.trace('create plan %s (%s)' % (body, stripe_info))
    Plan.__required_fields(meta, stripe_info)
    self.__meta = meta
    self.__id = None
    self.__stripe_info = Plan.__fill_missing_stripe_fields(stripe_info)
    self.__stripe_info['name'] = self.__stripe_info['name'].strip()
    self['team'] = True
    # Default storage.
    self['quotas'] = {'links': {'quota': int(100e9)}}
    self.update(body)
    self['creation_time'] = meta.now
    # Check properties.
    self.links_quota

  # ------- #
  # Saving. #
  # ------- #
  def save(self):
    """Int the plan to the database and save it on stripe.
    self['name'] has to be unique.
    """
    elle.log.trace('save plan to the database')
    import json
    try:
      plan_id = self.__meta.database.plans.insert(
        {
          'name': self.__stripe_info['name'],
        })
    except pymongo.errors.DuplicateKeyError:
      return Plan.conflict(self.__meta, self.__stripe_info['name'])
    elle.log.debug('id: %s' % plan_id)
    stripe_plan = stripe.Plan.create(
      api_key = self.__meta.stripe_api_key,
      id = str(plan_id),
      **self.__stripe_info
    )
    elle.log.debug('stripe plan: %s' % stripe_plan)
    self['stripe'] = stripe_plan
    elle.log.debug('%s' % self.__meta.view_plans())
    res = self.__meta.database.plans.find_and_modify(
      {'_id': plan_id},
      {'$set': self},
      new = True)
    elle.log.debug('result: %s' % res)
    return Plan.from_data(self.__meta, res)

  # -------- #
  # Finding. #
  # -------- #
  @staticmethod
  def find(meta, id, ensure_existence = False):
    elle.log.debug('find %s (ensure_existence: %s)' % (id, ensure_existence))
    return Plan._find(meta, {'_id': id}, ensure_existence = ensure_existence)

  @staticmethod
  def _find(meta, query, ensure_existence = False):
    """Return a plan from it's id.
    Raise of 404 if the plan is not present in the database and ensure_existence
    is set.

    id -- The plan id.
    ensure_existence -- Raise a 404 ou return None.
    """
    elle.log.debug('find %s (ensure_existence: %s)' % (id, ensure_existence))
    plan_db = meta.database.plans.find_one(query)
    if plan_db is None:
      if ensure_existence:
        return Plan.not_found(meta)
      return None
    return Plan.from_data(meta, plan_db)

  @staticmethod
  def from_data(meta, plan_db):
    """Cronstruct a Plan object from database data.

    meta -- The meta instance.
    plan_db -- The document.
    """
    res = Plan.__new__(Plan)
    res.__meta = meta
    res.__id = plan_db['_id']
    res.update(plan_db)
    res['id'] = plan_db['_id']
    del res['_id']
    return res

  # -------- #
  # Editing. #
  # -------- #
  editable_stripe_fields = ['name', 'metadata', 'statement_descriptor']
  @staticmethod
  def edit(meta, id, update):
    """Edit a plan.
    N.B. (from stripe documentation):
      'Updates the name of a plan. Other plan details (price, interval, etc.) are,
       by design, not editable.'

    id -- The team id.
    update -- A list of update to apply on the team.
    """
    elle.log.trace('edit %s (update: %s)' % (id, update))
    to_apply = {}
    save_plan = None
    if len(update.get('stripe_info', {})):
      for key in update['stripe_info']:
        if key not in Plan.editable_stripe_fields:
          return Plan.bad_request(mete, key)
      elle.log.trace('update stripe info')
      stripe_plan = stripe.Plan.retrieve(
        api_key = meta.stripe_api_key,
        id = str(id),
      )
      name_updated = False
      if stripe_plan is None:
        return Plan.not_found(meta)
      for key in update['stripe_info']:
        if key == 'name':
          update['name'] = update['stripe_info']['name']
        setattr(stripe_plan, key, update['stripe_info'][key])
      save_plan = lambda: stripe_plan.save()
      to_apply['stripe'] = stripe_plan
      elle.log.trace('updated stripe info: %s' % stripe_plan)
      del update['stripe_info']
    if len(update):
      to_apply.update(update)
    if len(to_apply):
      to_apply = {
        '$set': to_apply
      }
      elle.log.trace('update: %s' % update)
      elle.log.debug('%s' % meta.view_full_plans())
      try:
        res = meta.database.plans.find_and_modify(
          {'_id': id},
          to_apply,
          new = True,
          upsert = False)
        elle.log.debug('%s' % meta.view_full_plans())
        elle.log.trace('updated plan: %s' % res)
        if res is None:
          return Plan.not_found(meta)
        elle.log.debug('save stripe plan')
        if save_plan:
          save_plan()
        return Plan.from_data(meta, res)
      except pymongo.errors.DuplicateKeyError:
        return Plan.conflict(meta, update['name'])
    return Plan.find(meta, id = id, ensure_existence = True)
  # --------- #
  # Deleting. #
  # --------- #
  @staticmethod
  def delete(meta, id, force):
    """Delete the specified plan.
    Start by removing the plan from stripe. If the plan doesn't exist, triggers
    a 404 (except if force is present).
    Then remove the plan from the database. If the plan wasn't present, triggers
    a 404.

    id - The id of the plan.
    force - Ignore the stripe result.
    """
    elle.log.trace('delete plan %s (force: %s)' % (id, force))
    stripe_plan = stripe.Plan.retrieve(
      str(id),
      api_key = meta.stripe_api_key,
    )
    if stripe_plan is None and not force:
      return Plan.not_found(meta)
    if stripe_plan is not None:
      elle.log.trace('remove plan from stripe')
      stripe_plan.delete()
    elle.log.trace('remove plan from database')
    res = meta.database.plans.remove({'_id': id})
    if res['n'] == 1:
      return
    if res['n'] == 0:
      return Plan.not_found(meta)
    # Unreachable.
    assert False

  @property
  def view(self):
    """Return a view of the plan.
    """
    import copy
    res = copy.copy(self)
    res['name'] = self.name
    return sort_dict(res)

  @property
  def teams(self):
    """List the team currently using the plan.
    PS: This shouldn't be in plans (teams should have access to plan, plan
    shouldn't be aware of teams.)
    """
    return self.database.teams.find({'plan': self['id']})

  # ------------------------- #
  # Stripe fields compliance. #
  # ------------------------- #
  @staticmethod
  def __required_fields(meta, info):
    for field in ['name']:
      if field not in info:
        meta.bad_request(
          {
            'error': 'missing_parameter',
            'message': 'Missing mandatory key %s' % field
          })

  @staticmethod
  def __fill_missing_stripe_fields(stripe_info):
    """Following fields are required to create plans.
    """
    required_fields = {
      'interval': 'month',
      'currency': 'usd'
    }
    for field in required_fields:
      if field not in stripe_info:
        stripe_info[field] = required_fields[field]
    return stripe_info

  # ------ #
  # Errors #
  # ------ #
  @staticmethod
  def not_found(meta):
    return meta.not_found(
      {
        'error': 'plan_not_found',
        'reason': 'Plan doesn\'t exist'
      })

  @staticmethod
  def conflict(meta, name):
    return meta.conflict(
      {
        'error': 'plan_already_exists',
        'reason': 'A plan named %s already exists' % name,
      })

  @staticmethod
  def bad_request(meta, name):
    return meta.conflict(
      {
        'error': 'imutable_stripe_key',
        'reason': 'Key %s is not editable' % name,
      })

class Mixin:

  def __init__(self):
    self.__check_plans_integrity()

  @property
  def plans(self):
    plans = {}
    for plan in self.database.plans.find():
      plans[plan['name']] = plan
    # XXX: Dirty.
    plans[None] = plans['basic']
    return plans

  # -------------- #
  # Create a plan. #
  # -------------- #
  @api('/plans', method = 'POST')
  @require_admin
  def create_plan(self, body, stripe_info, team_plan : bool = True):
    return Plan(self, stripe_info = stripe_info, body = body,
                team = team_plan).save().view

  # ---------- #
  # Get plans. #
  # ---------- #
  def __plans(self, query = {}, fields = {}, team_plans_only = True):
    if team_plans_only:
      query['team'] = True
    return self.database.plans.find(query, fields)

  @api('/plans', method = 'GET')
  @require_admin
  def view_plans(self, team_plans_only : bool = True):
    return {'plans': [plan['_id'] for plan
                      in self.__plans(team_plans_only = team_plans_only)]}

  @api('/plans/full', method = 'GET')
  @require_admin
  def view_full_plans(self, team_plans_only : bool = True):
    return {'plans': [Plan.from_data(self, plan) for plan
                      in self.__plans(fields = None,
                                      team_plans_only = team_plans_only)]}

  # ----------- #
  # Get a plan. #
  # ----------- #
  @api('/plan/<id>', method = 'GET')
  @require_admin
  def view_plan(self, id: bson.ObjectId):
    return Plan.find(self, id, ensure_existence = True).view

  # -------------- #
  # Update a plan. #
  # -------------- #
  @api('/plan/<id>', method = 'PUT')
  @require_admin
  def update_plan(self, id: bson.ObjectId, update):
    return Plan.edit(self, id = id, update = update).view

  # -------------- #
  # Delete a plan. #
  # -------------- #
  @api('/plan/<id>', method = 'DELETE')
  @require_admin
  def delete_plan(self, id: bson.ObjectId, force = False):
    Plan.delete(self, id = id, force = force)
    return {}

  # ------ #
  # Uitls. #
  # ------ #
  def __check_plans_integrity(self):
    # Explore sub dictionnaries.
    def explore(input, d = {}):
      if not '.' in input:
        assert input in d.keys()
      else:
        p = input.split('.')
        explore('.'.join(p[1:]), d[p[0]])

    def check_field_existence(name):
      for plan in ['basic', 'plus', 'premium']:
        explore(name, self.plans[plan])

    def check_failure(field):
      try:
        check_field_existence(field)
        raise BaseException('test failed')
      except AssertionError as e:
        pass
    check_failure('unknown_field')
    check_failure('quotas.links.unknown_field')
    check_field_existence('quotas.p2p.size_limit')
    check_field_existence('quotas.links.bonuses.referrer')
    check_field_existence('quotas.send_to_self.bonuses.referrer')
