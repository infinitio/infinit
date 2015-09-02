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
    self['members'] = [admin['_id']]
    self['invitees'] = []
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

  def team_for_user(meta, user, pending = None, ensure_existence = False):
    if pending:
      query = {'_id': pending, 'invitees': user['_id']}
    else:
      query = {'members': user['_id']}
    return Team.find(meta, query, ensure_existence)

  def user_deleted(meta, user):
    user_id = user['_id']
    elle.log.trace('delete user from all teams: %s' % user_id)
    # Remove user from team.
    member_team = Team.team_for_user(meta, user)
    if member_team and user_id == member_team.admin_id:
      meta.forbidden(
        {
          'error': 'cant_delete_team_admin_user',
          'reason': 'User is admin of a team, delete the team first'
        })
    elif member_team:
      member_team.remove_member(user)
    # Remove user from invitees of all teams.
    meta.database.teams.update(
      {'invitees': user_id},
      {'$pull': {'invitees': user_id}})

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
  def __update_member_account(self, user):
    self.__meta._user_update(user, 'basic', force_prorata = True)

  def __cancel_stripe_subscription(self):
    elle.log.trace('cancel subscription for team: %s' % self.id)
    admin = self.__meta.user_from_identifier(self['admin'])
    with self.__meta._stripe:
      subscription = self.__meta._stripe.subscription(
        self.__meta._stripe.fetch_customer(admin))
      self.__meta._stripe.remove_plan(subscription)

  def add_invitee(self, invitee):
    user_id = invitee['_id']
    elle.log.trace('add invitee %s to team %s' % (user_id, self.id))
    res = self.__meta.database.teams.find_and_modify(
      {
        '_id': self.id,
        'invitees': {'$ne': user_id},
        'members': {'$ne': user_id},
      },
      {'$addToSet': {'invitees': user_id}},
      new = True,
      upsert = False)
    if res is None:
      elle.log.warn('unable to invite user %s, user is already invited' %
                    user_id)
      return self.__meta.bad_request(
        {
          'error': 'user_already_invited_to_team',
          'message': 'The user %s has already been invited' % user_id
        })
    return Team.from_data(self.__meta, res)

  def remove_invitee(self, invitee):
    user_id = invitee['_id']
    elle.log.trace('remove invitee %s from team %s' % (user_id, self.id))
    res = self.__meta.database.teams.find_and_modify(
      {'_id': self.id},
      {'$pull': {'invitees': user_id}},
      new = True)
    return Team.from_data(self.__meta, res)

  def add_member(self, user):
    user_id = user['_id']
    elle.log.trace('add member %s to team %s' % (user_id, self.id))
    try:
      res = self.__meta.database.teams.find_and_modify(
        {
          '_id': self.id,
          'invitees': user_id,
        },
        {
          '$push': {'members': user_id},
          '$pull': {'invitees': user_id},
        },
        new = True,
        upsert = False)
      if res is None:
        elle.log.warn(
          'unable to add user %s, user not invited or already member' % user_id)
        self.__meta.bad_request(
          {
            'error': 'user_in_team_or_not_invited',
            'message': 'User %s not invited or already member' % user_id
          })
      self.__update_member_account(user)
      self.__update_stripe_quantity(len(res['members']))
      return Team.from_data(self.__meta, res)
    except pymongo.errors.DuplicateKeyError as e:
      elle.log.warn(
        'unable to add user %s, user is already a member of another team' %
        user_id)
      return self.__meta.conflict(
        {
          'error': 'user_already_in_a_team',
          'message': 'The user %s is already in a team' % user_id
        })

  def remove_member(self, user):
    user_id = user['_id']
    elle.log.trace('remove member %s from team %s' % (user_id, self.id))
    res = self.__meta.database.teams.find_and_modify(
      {'_id': self['id']},
      {'$pull': {'members': user_id}},
      new = True)
    if res is None:
      elle.log.warn('unable to remove member %s' % user_id)
      self.__meta.bad_request
    self.__update_stripe_quantity(len(res['members']))
    return Team.from_data(self.__meta, res)

  def remove(self):
    elle.log.trace('remove team %s' % self.id)
    res = self.__meta.database.teams.remove(self.id)
    assert res['n'] == 1
    self.__cancel_stripe_subscription()

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
  def invitee_ids(self):
    return self['invitees']

  @property
  def member_ids(self):
    return self['members']

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
  def plan(self):
    return self['plan']

  @property
  def shared_settings(self):
    return self.get('shared_settings', {})

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
  def view(self):
    return {
      'admin': self.admin_id,
      'creation_time' : self.creation_time,
      'id': self.id,
      'invitees': self.invitee_ids,
      'members': self.member_ids,
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
      elle.log.warn('unable to create team %s' % name)
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
          'error': 'not_team_member',
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
    team.remove()
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
                     {'members': self.user['_id']},
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
  # Add invitee.
  # ============================================================================
  def __invite_team_member(self, team, invitee):
    return team.add_invitee(invitee).view

  @api('/team/invitees/<identifier>', method = 'PUT')
  @require_logged_in
  def add_team_invitee(self, identifier : utils.identifier):
    invitee = self.user_from_identifier(identifier)
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self.__invite_team_member(team, invitee)

  @api('/team/<team_id>/invitees/<user_identifier>', method = 'PUT')
  @require_admin
  def add_team_invitee_admin(self,
                             team_id : bson.ObjectId,
                             user_identifier : utils.identifier):
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    invitee = self.user_from_identifier(user_identifier)
    return team.__invite_team_member(team, invitee)

  # ============================================================================
  # Remove invitee.
  # ============================================================================
  def __remove_team_invitee(self, team, invitee):
    return team.remove_invitee(invitee).view

  @api('/team/invitees/<identifier>', method = 'DELETE')
  @require_logged_in
  def remove_team_invitee(self, identifier : utils.identifier):
    invitee = self.user_from_identifier(identifier)
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self.__remove_team_invitee(team, invitee)

  @api('/team/<team_id>/reject', method = 'POST')
  @require_logged_in
  def reject_team_invite(self, team_id : bson.ObjectId):
    user = self.user
    team = \
      Team.team_for_user(self, user, pending = team_id, ensure_existence = True)
    self.__remove_team_invitee(team, user)
    return {}

  @api('/team/<team_id>/invitees/<user_identifier>', method = 'DELETE')
  @require_admin
  def remove_team_invitee_admin(self,
                                team_id : bson.ObjectId,
                                user_identifier : utils.identifier):
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    invitee = self.user_from_identifier(user_identifier)
    return team.__remove_team_invitee(team, invitee)

  # ============================================================================
  # Add member.
  # ============================================================================
  def __add_team_member(self, team, member):
    return team.add_member(member).view

  @api('/team/<team_id>/join', method = 'POST')
  @require_logged_in
  def team_join(self, team_id : bson.ObjectId):
    user = self.user
    team = \
      Team.team_for_user(self, user, pending = team_id, ensure_existence = True)
    self.__add_team_member(team, user)
    return {}

  @api('/team/<team_id>/members/<user_identifier>', method = 'PUT')
  @require_logged_in
  def add_team_member_admin(self,
                             team_id : bson.ObjectId,
                             user_identifier : utils.identifier):
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    user = self.user_from_identifier(user_identifier)
    return team.__add_team_member(team, user)

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
    return team.remove_member(member)

  @api('/team/members/<identifier>', method = 'DELETE')
  @require_logged_in
  def remove_team_member(self, identifier : utils.identifier):
    member = self.user_from_identifier(identifier)
    team = Team.team_for_user(self, member, ensure_existence = True)
    self.__require_team_admin(team, self.user)
    return self.__remove_team_member(team, member)

  @api('/team/<team_id>/members/<user_identifier>', method = 'DELETE')
  @require_admin
  def remove_team_member(self,
                         team_id : bson.ObjectId,
                         user_identifier : utils.identifier):
    member = self.user_from_identifier(user_identifier)
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    return self.__remove_team_member(team, member)

  @api('/team/leave', method = 'POST')
  @require_logged_in_fields(['password'])
  def exit_a_team(self,
                  password):
    user = self.user
    self.__enforce_user_password(user, password)
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__remove_team_member(team, user)
    return {}

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
