# -*- encoding: utf-8 -*-

import elle.log
from elle.log import log, trace, debug, dump
from . import notifier, utils
from .utils import *
from .plans import Plan

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Team'

class Team(dict):

  def __init__(
      self,
      meta,
      admin,
      name,
      plan,
  ):
    name = name.strip()
    self.__meta = meta
    self.__id = None
    self['name'] = name
    self['lower_name'] = name.lower()
    self['plan'] = plan
    self['admin'] = admin['_id']
    self['members'] = [{'id': admin['_id'], 'status': 'ok'}]
    self['creation_time'] = meta.now
    self['modification_time'] = meta.now

  @staticmethod
  def from_data(meta, team_db):
    """Cronstruct a Team object from database data.

    meta -- The meta instance.
    team_db -- The document.
    """
    res = Team.__new__(Team)
    res.__meta = meta
    res.__id = team_db['_id']
    res.update(team_db)
    res['id'] = team_db['_id']
    del res['_id']
    return res

  @staticmethod
  def find(meta, query, ensure_existence = False):
    elle.log.debug('find %s (ensure_existence: %s)' % (
      query, ensure_existence))
    team = meta.database.teams.find_one(
      query,
    )
    if team is None:
      if ensure_existence:
        meta.not_found(
          {
            'error': 'unknown_team',
            'message': 'This team doesn\'t exist.'
          })
      else:
        return None
    return Team.from_data(team_db = team, meta = meta)

  def team_for_user(meta, user, ensure_existence = False, pending = None):
    query = {'members.id': user['_id']}
    if pending is not None:
      query['status'] = 'pending' if pending else 'ok'
    return Team.find(meta, query, ensure_existence)

  def edit(self, update):
    elle.log.trace('%s: edit (update: %s)' % (self, update))
    res = self.__meta.database.teams.find_and_modify(
      {'_id': self['id']},
      update,
      new = True)
    elle.log.debug('result: %s' % res)
    return Team.from_data(team_db = res, meta = self.__meta)

  def db_insert(self):
    self.__id = self.__meta.database.teams.insert(dict(self))
    self['id'] = self.__id
    return self

  def register_to_stripe(self, stripe_token, stripe_coupon = None):
    """Stripe needs an email address so the admin will be used as the
    subscription"""
    elle.log.trace('register to stripe %s (coupon: %s)' % (
      stripe_token, stripe_coupon))
    admin = self.__meta.user_from_identifier(self['admin'])
    elle.log.debug('plan: %s' % self['plan'])
    return self.__meta._user_update(
      admin, plan = self['plan'], stripe_token = stripe_token,
      stripe_coupon = stripe_coupon)

  def __update_stripe_quantity(self, quantity):
    elle.log.trace('update subscrition quantity to %s' % quantity)
    admin = self.__meta.user_from_identifier(self['admin'])
    with self.__meta._stripe:
      subscription = self.__meta._stripe.subscription(
        self.__meta._stripe.fetch_customer(admin))
      if quantity != subscription.quantity:
        subscription.quantity = quantity
        subscription.save()

  def __manages_users(self, operation, user_id):
    res = self.__meta.database.teams.find_and_modify(
      {'_id': self['id']},
      {operation: {'members': {'id': user_id, 'status': 'ok'}}},
      new = True,
      upsert = False)
    self.__update_stripe_quantity(len(res['members']))
    return res

  def add_user(self, invitee):
    elle.log.trace('add user %s' % invitee['_id'])
    old_team = Team.team_for_user(self.__meta, invitee)
    if old_team:
      return self.__meta.forbidden(
        {
          'error': 'user_already_in_a_team',
          'message': 'The user %s is already in a team'
        })
    res = self.__manages_users('$addToSet', invitee['_id'])
    return Team.from_data(self.__meta, res)

  def remove_user(self, user):
    elle.log.trace('remove user %s' % user['_id'])
    old_team = Team.team_for_user(self.__meta, user, pending = False)
    res = self.__manages_users('$pull', user['_id'])
    return Team.from_data(self.__meta, res)

  @property
  def storage_used(self):
    res = self.__meta.database.links.aggregate([
      {'$match': {
        'sender_id': {'$in': self.member_ids},
        'quota_counted': True}
      },
      {'$group': {
        '_id': None,
        'total': {'$sum': '$file_size'}}
      }
    ])['result']
    return res[0]['total'] if len(res) else 0

  @property
  def admin_id(self):
    return self['admin']

  @property
  def admin_user(self):
    return self.__meta.user_by_id(self.admin_id)

  @property
  def creation_time(self):
    return self['creation_time']

  @property
  def id(self):
    return self.__id

  @property
  def member_ids(self):
    return [m['id'] for m in self['members']]

  @property
  def modification_time(self):
    return self['modification_time']

  @property
  def member_users(self):
    return self.__meta.users_by_ids(self.member_ids)

  @property
  def name(self):
    return self['name']

  @property
  def shared_settings(self):
    return self.get('shared_settings', {})

  @property
  def plan(self):
    return self['plan']

  @property
  def view(self):
    return {
      'admin': self.admin_id,
      'creation_time' : self.creation_time,
      'id': self.id,
      'members': self.member_users,
      'modification_time' : self.modification_time,
      'name' : self.name,
      'storage_used': self.storage_used,
    }

class Mixin:
  # ============================================================================
  # Creation.
  # ============================================================================
  def __create_team(self, owner, name,
                    stripe_token,
                    plan,
                    stripe_coupon = None):
    elle.log.trace('create team %s (plan: %s)' % (name, plan))
    plan = Plan.find(self, plan, ensure_existence = True)
    try:
      team = Team(self, owner, name, plan).db_insert()
      team.register_to_stripe(stripe_token, stripe_coupon)
    except pymongo.errors.DuplicateKeyError:
      elle.log.dump('teams: %s' % list(self.database.teams.find()))
      return self.conflict(
        {
          'error': 'team_name_already_taken',
          'reason': 'A team named %s already exists' % name
        })
    assert team is not None
    return team

  @api('/teams', method = 'POST')
  @require_logged_in
  def create_team(self,
                  name,
                  stripe_token,
                  plan : Plan.translate_in):
    user = self.user
    team = Team.team_for_user(self, user)
    if team:
      self.forbidden(
        {
          'error': 'already_in_a_team',
          'reason': 'You already are in team %s.' % team['name'],
        })
    return self.__create_team(user, name, stripe_token = stripe_token,
                              plan = plan)

  # @api('/team/<name>', method = 'PUT')
  # def create_team(self,
  #                 name,
  #                 new_name = None,
  #                 new_admin =
  #                 stripe_token = None):
  #   team = Team.find(self, {'name': name})
  #   self.user = user
  #   if team == None: # Creation.
  #     self.__create_team(user, name)
  #   else:
  #     update = {}
  #     if new_name:
  #       update.update{'name': name}
  #     try:
  #       team.edit({'$set': update})
  #     except pymongo.errors.DuplicateKeyError:
  #       return self.conflict(
  #         {
  #           'error': 'team_name_already_taken',
  #           'reason': 'A team named %s already exists' % name
  #         })

  # ============================================================================
  # Deletion.
  # ============================================================================
  def __require_team_admin(self, team, user):
    if team.admin_id != user['_id']:
      return self.forbidden(
        {
          'error': 'not_the_team_admin',
          'reason': 'Only the team administrator can edit or delete a team.'
        })

  def __require_team_member(self, team, user):
    if user['_id'] not in team.member_ids:
      return self.forbidden(
        {
          'error': 'not_team_memeber',
          'reason': 'Only team members can access team properties'
        })

  def __enforce_user_password(self, user, password):
    if user['password'] != hash_password(password):
      return self.unauthorized(
        {
          'error': 'wrong_password',
          'message': 'Password don\'t match.'
        })

  def __delete_specific_team(self, team):
    res = self.database.teams.remove(team.id)
    assert res['n'] == 1
    return {}

  @api('/team', method = 'DELETE')
  @require_logged_in_fields(['password'])
  def delete_team(self, password):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    self.__enforce_user_password(user, password)
    return self.__delete_specific_team(team)

  @api('/team/<identifier>', method = 'DELETE')
  @require_admin
  def delete_specific_team(self, identifier):
    team = Team.find(self, identifier = identifier)
    return self.__delete_specific_team(self, team)

  # ============================================================================
  # View.
  # ============================================================================
  @api('/team', method = 'GET')
  @require_logged_in
  def team_view(self):
    team = Team.find(self,
                     {'members.id': self.user['_id']},
                     ensure_existence = True)
    return team.view

  @api('/team/<identifier>', method = 'GET')
  @require_admin
  def team_view_admin(self, identifier: bson.ObjectId):
    team = Team.find(self, {'_id': identifier}, ensure_existence = True)
    return team.view

  # ============================================================================
  # Update.
  # ============================================================================
  @api('/team', method = 'PUT')
  @require_logged_in
  def update_team(self, name):
    team = Team.team_for_user(self, self.user, ensure_existence = True)
    return team.edit({'$set': {'name': name, 'lower_name': name.lower()}}).view

  @api('/team/<identifier>', method = 'PUT')
  @require_admin
  def update_team_admin(self, identifier : bson.ObjectId, name):
    team = Team.find(self, {'_id': identifier}, ensure_existence = True)
    return team.edit({'$set': {'name': name, 'lower_name': name.lower()}}).view

  # ============================================================================
  # Add member.
  # ============================================================================
  def __add_team_member(self, team, invitee):
    return team.add_user(invitee).view
    # XXX: I wish I could make that work.
    # try:
    #   team = team.edit({'$addToSet': {'members': invitee['_id']}})
    # except pymongo.errors.DuplicateKeyError:
    #   return self.forbidden(
    #     {
    #       'error': 'user_already_in_a_team',
    #       'message': 'The user %s is already in a team'
    #     })

  @api('/team/members/<identifier>', method = 'PUT')
  @require_logged_in
  def add_team_member(self, identifier : utils.identifier):
    invitee = self.user_from_identifier(identifier)
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self.__add_team_member(team, invitee)

  @api('/team/<team_id>/members/<user_identifier>', method = 'PUT')
  @require_admin
  def add_team_member_admin(self,
                            team_id : bson.ObjectId,
                            user_identifier : utils.identifier):
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    invitee = self.user_from_identifier(user_identifier)
    return team.__add_team_member(team, invitee['_id'])

  # ============================================================================
  # Remove member.
  # ============================================================================
  def __remove_team_member(self, team, member):
    if member['_id'] not in team.member_ids:
      return self.not_found(
        {
          'error': 'unknown_user',
          'message': 'The user %s is not in the team' % member['_id'],
        })
    return team.remove_user(member)

  @api('/team/members/<identifier>', method = 'DELETE')
  @require_logged_in
  def remove_team_member(self, identifier : utils.identifier):
    member = self.user_from_identifier(identifier)
    user = self.user
    team = Team.team_for_user(self, member, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self.__remove_team_member(team, member)

  @api('/team/<team_id>/members/<user_identifier>', method = 'DELETE')
  @require_admin
  def remove_team_member(self,
                         team_id : bson.ObjectId,
                         user_identifier : utils.identifier):
    member = self.user_from_identifier(user_identifier)
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    return self.__remove_team_member(team, member)

  # XXX: Rename that
  @api('/team/leave', method = 'POST')
  @require_logged_in_fields(['password'])
  def exit_a_team(self,
                  password):
    user = self.user
    self.__enforce_user_password(user, password)
    team = Team.team_for_user(self, user, ensure_existence = True)
    return self.__remove_team_member(team, user)

  # ============================================================================
  # Shared settings.
  # ============================================================================
  @api('/team/shared_settings', method = 'GET')
  @require_logged_in
  def team_shared_settings_get_api(self):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_member(team, user)
    return team.shared_settings

  @api('/team/shared_settings', method = 'POST')
  @require_logged_in
  def team_shared_settings_post_api(self, **kwargs):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    update = {}
    for field in [
      'default_background',
    ]:
      if field in kwargs:
        update[field] = kwargs[field]
    team = team.edit({'$set': {
      'shared_settings.%s' % field: value for field, value in update.items()}})
    return team.shared_settings

  @api('/team/backgrounds/<name>', method = 'PUT')
  @require_logged_in
  def team_background_put_api(self, name):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self._cloud_image_upload(
      'team_backgrounds', name, team = team)

  @api('/team/backgrounds/<name>', method = 'GET')
  @require_logged_in
  def team_background_get_api(self, name):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_member(team, user)
    return self.team_background_get(team, name)

  @api('/team/<team_id>/backgrounds/<name>', method = 'GET')
  def team_background_get_api(self, team_id: bson.ObjectId, name):
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    return self.team_background_get(team, name)

  def team_background_get(self, team, name):
    return self._cloud_image_get('team_backgrounds', name, team = team)

  @api('/team/backgrounds/<name>', method = 'DELETE')
  @require_logged_in
  def team_background_delete_api(self, name):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self._cloud_image_delete('team_backgrounds', name, team = team)

  @api('/team/backgrounds', method = 'GET')
  @require_logged_in
  def team_background_list_api(self):
    team = Team.team_for_user(self, self.user, ensure_existence = True)
    self._check_gcs()
    return {
      'backgrounds': self.gcs.bucket_list('team_backgrounds', prefix = team.id),
    }

  @api('/team/custom_domains/<name>', method = 'PUT')
  @require_logged_in
  def set_team_domain(self, name):
    team = Team.team_for_user(self, self.user, ensure_existence = True)
    self.__require_team_admin(team, self.user)
    old, new = self._custom_domain_edit(name, 'add', team)
    if old is None:
      bottle.response.status = 201
    return new

  @api('/team/custom_domains/<name>', method = 'DELETE')
  @require_logged_in
  def user_account_patch_api(self, name):
    team = Team.team_for_user(self, self.user, ensure_existence = True)
    self.__require_team_admin(team, self.user)
    old, new = self._custom_domain_edit(name, 'remove', team)
    if old is None:
      self.not_found({
        'reason': 'custom domain %s not found' % name,
        'custom-domain': name,
      })
    return new
