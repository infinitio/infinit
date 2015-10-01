# -*- encoding: utf-8 -*-

import elle.log
from elle.log import log, trace, debug, dump
from . import notifier, transaction_status, utils
import infinit.oracles.emailer
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
    interval = 'month',
    step = 1
  ):
    name = name.strip()
    self.__meta = meta
    self.__id = None
    self['name'] = name
    self['lower_name'] = name.lower()
    self['plan'] = plan
    self['admin'] = admin['_id']
    now = meta.now
    self['members'] = [Team.user_id_time_pair(admin['_id'], now)]
    self['invitees'] = []
    self['creation_time'] = now
    self['modification_time'] = now
    self['interval'] = interval
    self['step'] = step

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
            'reason': 'This team doesn\'t exist.'
          })
      else:
        return None
    return Team.from_data(team_db = team, meta = meta)

  @staticmethod
  def user_id_time_pair(user_id, time):
    return sort_dict({'id': user_id, 'since': time})

  @staticmethod
  def user_deleted(meta, user):
    user_id = user['_id']
    elle.log.trace('delete user from all teams: %s' % user_id)
    # Remove user from team.
    member_team = Team.team_for_user(meta, user)
    if member_team and user_id == member_team.admin_id:
      meta.forbidden(
        {
          'error': 'cant_delete_team_admin',
          'reason': 'User is admin of a team, delete the team first.'
        })
    elif member_team:
      member_team.remove_member(user)
    # Remove user from invitees of all teams.
    meta.database.teams.update(
      {'invitees.id': user_id},
      {
        '$pull': {'invitees': {'id': user_id}},
        '$set': {'modification_time': meta.now},
      })

  def team_for_user(meta, user, pending = None, ensure_existence = False):

    def __find_team_for_user(meta, user, pending, ensure_existence):
      if pending:
        emails = [a['type'] == 'email' and a['id'] for a in user['accounts']]
        emails = list(filter(lambda e: e is not False, emails))
        query = {'_id': pending, 'invitees.id': {'$in': [user['_id']] + emails}}
      else:
        query = {'members.id': user['_id']}
      return Team.find(meta, query, ensure_existence)

    if hasattr(bottle.request, 'user'):
      if bottle.request.user['_id'] != user['_id']:
        return __find_team_for_user(meta, user, pending, ensure_existence)
    if hasattr(bottle.request, 'team') and bottle.request.team is not False:
      return bottle.request.team
    team = __find_team_for_user(meta, user, pending, ensure_existence)
    if hasattr(bottle.request, 'session'):
      bottle.request.team = team
    return team

  def __clear_cached_team(self, user):
    if hasattr(bottle.request, 'user'):
      if bottle.request.user['_id'] == user['_id']:
        if hasattr(bottle.request, 'team'):
          bottle.request.team = False

  def edit(self, update):
    elle.log.trace('%s: edit (update: %s)' % (self, update))
    if update.get('$set'):
      update['$set'].update({'modification_time': self.__meta.now})
    else:
      update['$set'] = {'modification_time': self.__meta.now}
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

  def register_to_stripe(self, stripe_token, interval = None, step = None,
                         stripe_coupon = None):
    """Stripe needs an email address so the admin will be used as the
    subscription"""
    elle.log.trace('register to stripe %s (coupon: %s)' % (
      stripe_token, stripe_coupon))
    admin = self.__meta.user_from_identifier(self.admin_id)
    self.__clear_cached_team(admin)
    elle.log.debug('plan: %s' % self.plan_id)
    return self.__meta._user_update(
      admin, plan = self.plan_id, stripe_token = stripe_token,
      stripe_coupon = stripe_coupon, interval = interval, step = step)

  def __update_stripe_quantity(self, quantity):
    elle.log.trace('update subscrition quantity to %s' % quantity)
    admin = self.__meta.user_from_identifier(self['admin'])
    with self.__meta._stripe:
      subscription = self.__meta._stripe.subscription(
        self.__meta._stripe.fetch_customer(admin))
      if quantity != subscription.quantity:
        subscription.quantity = quantity
        subscription.save()

  def __cancel_stripe_subscription(self):
    elle.log.trace('cancel subscription for team: %s' % self.id)
    admin = self.__meta.user_from_identifier(self['admin'])
    with self.__meta._stripe:
      subscription = self.__meta._stripe.subscription(
        self.__meta._stripe.fetch_customer(admin))
      self.__meta._stripe.remove_plan(subscription)

  def add_invitee(self, invitee):
    is_user = isinstance(invitee, dict)
    identifier = invitee['_id'] if is_user else invitee
    elle.log.trace('add invitee %s to team %s' % (identifier, self.id))
    now = self.__meta.now
    res = self.__meta.database.teams.find_and_modify(
      {
        '_id': self.id,
        'invitees.id': {'$ne': identifier},
        'members.id': {'$ne': identifier},
      },
      {
        '$push': {'invitees': Team.user_id_time_pair(identifier, now)},
        '$set': {'modification_time': now},
      },
      new = True,
      upsert = False)
    if res is None:
      elle.log.warn('unable to invite user %s, user is already invited' %
                    identifier)
      return self.__meta.bad_request(
        {
          'error': 'user_already_invited_to_team',
          'reason': 'The user (%s) has already been invited.' %
            invitee['email'] if is_user else invitee
        })
    variables = {
      'admin': self.__meta.email_user_vars(self.admin_user),
      'key': utils.key('/teams/%s' % self.id),
      'team': {'id': self.id, 'name': self.name}
    }
    if is_user:
      variables.update({
        'user': self.__meta.email_user_vars(invitee),
        'login_token': self.__meta.login_token(invitee['email']),
      })
    else:
      variables.update({'ghost_email': invitee})
    self.__meta.emailer.send_one(
      'Join',
      recipient_email = invitee['email'] if is_user else invitee,
      recipient_name = invitee['fullname'] if is_user else invitee,
      variables = variables)
    return Team.from_data(self.__meta, res)

  def remove_invitee(self, invitee):
    identifier = invitee['_id'] if isinstance(invitee, dict) else invitee
    elle.log.trace('remove invitee %s from team %s' % (identifier, self.id))
    res = self.__meta.database.teams.find_and_modify(
      {'_id': self.id},
      {
        '$pull': {'invitees': {'id': identifier}},
        '$set': {'modification_time': self.__meta.now},
      },
      new = True)
    return Team.from_data(self.__meta, res)

  def __notify_quota_change(self):
    self.__meta._quota_updated_notify(self.admin_user, team = self)

  def add_member(self, user):
    user_id = user['_id']
    emails = [a['type'] == 'email' and a['id'] for a in user['accounts']]
    emails = list(filter(lambda e: e is not False, emails))
    elle.log.trace('add member %s to team %s' % (user_id, self.id))
    try:
      now = self.__meta.now
      res = self.__meta.database.teams.find_and_modify(
        {
          '_id': self.id,
          'invitees.id': {'$in': [user_id] + emails},
        },
        {
          '$push': {'members': Team.user_id_time_pair(user_id, now)},
          '$pull': {'invitees': {'id': {'$in': [user_id] + emails}}},
          '$set': {'modification_time': now},
        },
        new = True,
        upsert = False,)
      if res is None:
        elle.log.warn(
          'unable to add user %s, user not invited or already member' % user_id)
        self.__meta.bad_request(
          {
            'error': 'user_in_team_or_not_invited',
            'reason': 'User (%s) not invited or already member.' % user['email']
          })
      self.__clear_cached_team(user)
      self.__update_stripe_quantity(len(res['members']))
      self.__meta._user_update(
        user, self.plan_id, force_prorata = True, team_member = True)
      self.__notify_quota_change()
      self.__meta.emailer.send_one(
        'Joined',
        recipient_email = self.admin_user['email'],
        recipient_name = self.admin_user['fullname'],
        variables = {
          'admin': self.__meta.email_user_vars(self.admin_user),
          'login_token': self.__meta.login_token(self.admin_user['email']),
          'members': self.members_view,
          'team': {
            'id': self.id,
            'members': self.members_view,
            'name': self.name,
          },
          'user': self.__meta.email_user_vars(user),
        }
      )
      return Team.from_data(self.__meta, res)
    except pymongo.errors.DuplicateKeyError as e:
      elle.log.warn(
        'unable to add user %s, user is already a member of another team' %
        user_id)
      return self.__meta.conflict(
        {
          'error': 'already_in_a_team',
          'reason': 'The user (%s) is already in a team.' % user_id
        })

  def remove_member(self, user):
    user_id = user['_id']
    elle.log.trace('remove member %s from team %s' % (user_id, self.id))
    res = self.__meta.database.teams.find_and_modify(
      {'_id': self['id']},
      {
        '$pull': {'members': {'id': user_id}},
        '$set': {'modification_time': self.__meta.now},
      },
      new = True)
    if res is None:
      elle.log.warn('unable to remove member %s' % user_id)
      self.__meta.bad_request(
        {
          'error': 'not_team_member',
          'reason': 'The user (%s) is not part of team (%s).' %
            (user['email'], self.id)
        })
    self.__clear_cached_team(user)
    self.__update_stripe_quantity(len(res['members']))
    self.__meta._user_update(user, 'basic', team_member = True)
    # Ensure removed user is not updated as part of the team.
    self['members'] = [m for m in self['members'] if m['id'] != user_id]
    self.__notify_quota_change()
    return Team.from_data(self.__meta, res)

  def remove(self):
    elle.log.trace('remove team %s' % self.id)
    res = self.__meta.database.teams.remove(self.id)
    assert res['n'] == 1
    self.__cancel_stripe_subscription()
    for user in self.member_users:
      self.__clear_cached_team(user)
      self.__meta._user_update(user, 'basic', team_member = True)

  @property
  def admin_id(self):
    return self['admin']

  @property
  def admin_user(self):
    return self.__meta.user_from_identifier(self.admin_id)

  @property
  def creation_time(self):
    return self['creation_time']

  @property
  def id(self):
    return self.__id

  @property
  def invitees_view(self):
    res = []
    for element in self['invitees']:
      if isinstance(element['id'], bson.ObjectId):
        user = self.__meta.user_from_identifier(element['id'],
                                                fields = ['email', 'fullname'])
        element.update(user)
        del element['_id']
      res.append(element)
    return res

  @property
  def member_count(self):
    return len(self.member_ids)

  @property
  def member_ids(self):
    return [m['id'] for m in self['members']]

  @property
  def member_users(self):
    return self.__meta.users_by_ids(self.member_ids)

  @property
  def members(self):
    return self['members']

  @property
  def members_view(self):
    res = []
    for element in self['members']:
      user = self.__meta.user_from_identifier(element['id'],
                                              fields = ['email', 'fullname'])
      del user['_id']
      element.update(user)
      res.append(element)
    return res

  @property
  def modification_time(self):
    return self['modification_time']

  @property
  def name(self):
    return self['name']

  @property
  def plan(self):
    return Plan.find(self.__meta, self.plan_id)

  @property
  def plan_id(self):
    return self['plan']

  @property
  def quotas(self):
    res = self.plan['quotas']
    if res['links'].get('per_user_storage'):
      res['links'].update({
        'shared_storage':
          int(res['links']['per_user_storage'] * self.member_count)})
    return res

  @property
  def shared_settings(self):
    return self.get('shared_settings', {})

  @property
  def storage_used(self):
    res = self.__meta.database.links.aggregate([
      {'$match': {
        'sender_id': {'$in': self.member_ids},
        'status': transaction_status.FINISHED,
        'quota_counted': True}
      },
      {'$group': {
        '_id': None,
        'total': {'$sum': '$file_size'}}
      }
    ])['result']
    return int(res[0]['total']) if len(res) else 0

  @property
  def view(self):
    admin_user = self.admin_user
    return {
      'admin': {
        'email': admin_user['email'],
        'fullname': admin_user['fullname'],
        'id': admin_user['_id'],
      },
      'creation_time' : self.creation_time,
      'id': self.id,
      'invitees': self.invitees_view,
      'members': self.members_view,
      'modification_time' : self.modification_time,
      'name' : self.name,
      'storage_used': self.storage_used,
    }

class Mixin:
  # ============================================================================
  # Helpers.
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
          'reason': 'Only team members can access team properties.'
        })

  def __enforce_user_password(self, user, password):
    if user['password'] != hash_password(password):
      return self.unauthorized(
        {
          'error': 'wrong_password',
          'reason': 'Incorrect password.'
        })

  # ============================================================================
  # Creation.
  # ============================================================================
  def __create_team(self, owner, name,
                    stripe_token,
                    plan,
                    interval = 'month',
                    step = 1,
                    stripe_coupon = None):
    elle.log.trace('create team %s (plan: %s)' % (name, plan))
    Plan.find(self, plan, ensure_existence = True)
    team = Team(self, owner, name, plan, interval, step).db_insert()
    assert team is not None
    team.register_to_stripe(stripe_token, interval, step, stripe_coupon)
    return team

  @api('/teams', method = 'POST')
  @require_logged_in
  def create_team(self,
                  name,
                  stripe_token,
                  plan : Plan.translate_in,
                  interval = 'month',
                  step = 1,
                  stripe_coupon = None):
    user = self.user
    team = Team.team_for_user(self, user)
    if team:
      return self.forbidden(
        {
          'error': 'already_in_a_team',
          'reason': 'You already are in team %s.' % team['name'],
        })
    elle.log.debug('create_team: plan: %s, name: %s, stripe_token: %s' %
        (plan, name, stripe_token))
    if plan is None or name is None or stripe_token is None:
      return self.bad_request({
        'error': 'missing_fields',
        'reason': 'Plan, name or stripe_token missing.'})
    return self.__create_team(user, name, stripe_token = stripe_token,
                              plan = plan, interval = interval, step = step,
                              stripe_coupon = stripe_coupon)

  # ============================================================================
  # Deletion.
  # ============================================================================
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

  @api('/teams/<identifier>', method = 'DELETE')
  @require_admin
  def delete_specific_team(self, identifier):
    team = Team.find(self, identifier = identifier)
    return self.__delete_specific_team(team)

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

  @api('/teams/<identifier>', method = 'GET')
  @require_key
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
    self.__require_team_admin(team, self.user)
    return team.edit({'$set': {'name': name, 'lower_name': name.lower()}}).view

  @api('/teams/<identifier>', method = 'PUT')
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
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    invitee = self.user_from_identifier(identifier,
                                        fields = ['_id', 'email', 'fullname'])
    if invitee is None:
      if utils.is_an_email_address(identifier):
        invitee = identifier
      else:
        return self.bad_request({
          'error': 'email_not_valid',
          'reason': 'No user was found with the identifier (%s) and it is not \
                     a valid email address.' % identifier,
        })
    return self.__invite_team_member(team, invitee)

  @api('/teams/<team_id>/invitees/<user_identifier>', method = 'PUT')
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
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    invitee = self.user_from_identifier(identifier)
    if invitee is None:
      if utils.is_an_email_address(identifier):
        invitee = identifier
      else:
        return self.bad_request({
          'error': 'email_not_valid',
          'reason': 'No user was found with the identifier (%s) and it is not \
                     a valid email address.' % identifier,
        })
    return self.__remove_team_invitee(team, invitee)

  @api('/teams/<team_id>/reject', method = 'POST')
  @require_logged_in
  def reject_team_invite(self, team_id : bson.ObjectId):
    user = self.user
    team = \
      Team.team_for_user(self, user, pending = team_id, ensure_existence = True)
    self.__remove_team_invitee(team, user)
    return {}

  @api('/teams/<team_id>/invitees/<user_identifier>', method = 'DELETE')
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

  @api('/teams/<team_id>/join', method = 'POST')
  @require_logged_in
  def team_join(self, team_id : bson.ObjectId):
    user = self.user
    team = \
      Team.team_for_user(self, user, pending = team_id, ensure_existence = True)
    self.__add_team_member(team, user)
    return {}

  @api('/teams/<team_id>/members/<user_identifier>', method = 'PUT')
  @require_admin
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
          'reason': 'The user (%s) is not in the team.' % member['_id'],
        })
    return team.remove_member(member)

  @api('/team/members/<identifier>', method = 'DELETE')
  @require_logged_in
  def remove_team_member(self, identifier : utils.identifier):
    member = self.user_from_identifier(identifier,
                                       fields = ['_id', 'email', 'fullname'])
    team = Team.team_for_user(self, member, ensure_existence = True)
    self.__require_team_admin(team, self.user)
    res = self.__remove_team_member(team, member)
    self.emailer.send_one(
      'Kicked Out',
      recipient_email = member['email'],
      recipient_name = member['fullname'],
      variables = {
        'admin': self.email_user_vars(team.admin_user),
        'login_token': self.login_token(member['email']),
        'team': {'id': team.id, 'name': team.name},
        'user': self.email_user_vars(member),
      }
    )
    return res

  @api('/teams/<team_id>/members/<user_identifier>', method = 'DELETE')
  @require_admin
  def remove_team_member(self,
                         team_id : bson.ObjectId,
                         user_identifier : utils.identifier):
    member = self.user_from_identifier(user_identifier)
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    return self.__remove_team_member(team, member)

  @api('/team/leave', method = 'POST')
  @require_logged_in_fields(['password'])
  def exit_a_team(self, password):
    user = self.user
    self.__enforce_user_password(user, password)
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__remove_team_member(team, user)
    self.emailer.send_one(
      'Left',
      recipient_email = team.admin_user['email'],
      recipient_name = team.admin_user['fullname'],
      variables = {
        'admin': self.email_user_vars(team.admin_user),
        'login_token': self.login_token(team.admin_user['email']),
        'team': {
          'id': team.id,
          'members': team.members_view,
          'name': team.name,
        },
        'user': self.email_user_vars(user),
      }
    )
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

  ## ----------- ##
  ## Backgrounds ##
  ## ----------- ##

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

  @api('/teams/<team_id>/backgrounds/<name>', method = 'GET')
  def teams_background_get_api(self, team_id: bson.ObjectId, name):
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

  ## -------------- ##
  ## Custom Domains ##
  ## -------------- ##

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

  ## ---- ##
  ## Logo ##
  ## ---- ##

  @api('/team/logo', method = 'PUT')
  @require_logged_in
  def team_logo_put_api(self):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self._cloud_image_upload('team_logo', None, team = team)

  @api('/team/logo', method = 'GET')
  @require_logged_in
  def team_logo_get_api(self, cache_buster = None):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_member(team, user)
    return self.team_logo_get(team, cache_buster)

  @api('/teams/<team_id>/logo', method = 'GET')
  def teams_logo_get_api(self, team_id: bson.ObjectId, cache_buster = None):
    team = Team.find(self, {'_id': team_id}, ensure_existence = True)
    return self.team_logo_get(team, cache_buster)

  def team_logo_get(self, team, cache_buster = None):
    return self._cloud_image_get('team_logo', None, team = team)

  @api('/team/logo', method = 'DELETE')
  @require_logged_in
  def team_logo_delete_api(self):
    user = self.user
    team = Team.team_for_user(self, user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self._cloud_image_delete('team_logo', None, team = team)
