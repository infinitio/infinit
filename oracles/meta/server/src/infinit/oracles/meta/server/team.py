# -*- encoding: utf-8 -*-

import elle.log
from elle.log import log, trace, debug, dump
from . import utils
from .utils import *

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Team'

class Team(dict):

  def __init__(
      self,
      meta,
      admin,
      name,
  ):
    self.__meta = meta
    self.__id = None
    self['name'] = name
    self['admin'] = admin['_id']
    self['members'] = [admin['_id']]
    self['creation_time'] = meta.now
    self['modification_time'] = meta.now

  @staticmethod
  def __build(team_db, meta):
    res = Team.__new__(Team)
    res.__meta = meta
    res.__id = team_db['_id']
    res.update(team_db)
    res['id'] = team_db['_id']
    del res['_id']
    return res

  @staticmethod
  def find(meta, query, ensure_existence = False):
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
    return Team.__build(team_db = team, meta = meta)

  def edit(self, update):
    res = self.__meta.database.teams.find_and_modify(
      {'_id': self['id']},
      update,
      new = True)
    return Team.__build(team_db = res, meta = self.__meta)

  def db_insert(self):
    self.__id = self.__meta.database.teams.insert(dict(self))
    self['id'] = self.__id
    return self

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
  # Search.
  # ============================================================================
  def team_for_user(self, user, ensure_existence = False):
    return Team.find(self, {'members': user['_id']}, ensure_existence)

  # ============================================================================
  # Creation.
  # ============================================================================
  def __create_team(self, owner, name):
    try:
      team = Team(self, owner, name).db_insert()
    except pymongo.errors.DuplicateKeyError:
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
                  stripe_token):
    user = self.user
    team = self.team_for_user(user)
    if team:
      self.forbidden(
        {
          'error': 'already_in_a_team',
          'reason': 'You already are in team %s' % team['name'],
        })
    return self.__create_team(user, name)

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
          'reason': 'Only the team admin can edit or delete a team'
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
    team = self.team_for_user(user, ensure_existence = True)
    self.__require_team_admin(team, user)
    self.__enforce_user_password(user, password)
    return self.__delete_specific_team(team)

  @api('/team/<id>', method = 'DELETE')
  @require_admin
  def delete_specific_team(self, id):
    team = Team.find(self, id = id)
    return self.__delete_specific_team(self, team)

  # ============================================================================
  # View.
  # ============================================================================
  @api('/team', method = 'GET')
  @require_logged_in
  def team_view(self):
    user = self.user
    team = Team.find(self, {'members': user['_id']}, ensure_existence = True)
    return team.view

  @api('/team/<id>', method = 'GET')
  @require_admin
  def team_view_admin(self, id: bson.ObjectId):
    team = Team.find(self, {'_id': id}, ensure_existence = True)
    return team.view

  # ============================================================================
  # Update.
  # ============================================================================
  @api('/team', method = 'PUT')
  @require_logged_in
  def update_team(self, name):
    team = self.team_for_user(self.user, ensure_existence = True)
    return team.edit({'$set': {'name': name}}).view

  @api('/team/<id>', method = 'PUT')
  @require_admin
  def update_team_admin(self, id, name):
    team = Team.find(self, {'_id': id}, ensure_existence = True)
    return Team.edit({'$set': {'name': name}}).view

  # ============================================================================
  # Add member.
  # ============================================================================
  def __add_team_member(self, team, invitee):
    old_team = self.team_for_user(invitee)
    if old_team:
      return self.forbidden(
        {
          'error': 'user_already_in_a_team',
          'message': 'The user %s is already in a team'
        })
    return team.edit({'$addToSet': {'members': invitee['_id']}}).view
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
    team = self.team_for_user(user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self.__add_team_member(team, invitee)

  @api('/team/<team_id>/members/<user_identifier>', method = 'PUT')
  @require_admin
  def add_team_member_admin(self, team_id, user_identifier : utils.identifier):
    team = Team.find(self, {'_id': bson.ObjectId(team_id)})
    invitee = self.user_from_identifier(identifier)
    return self.__add_team_member(team, invitee)

  # ============================================================================
  # Remove member.
  # ============================================================================
  def __remove_team_member(self, team, member):
    if member['_id'] not in team['members']:
      return self.not_found(
        {
          'error': 'unknown user',
          'message': 'The user %s is not in the team' % identifier
        })
    return team.edit({'$pull': {'members': member['_id']}}).view

  @api('/team/members/<identifier>', method = 'DELETE')
  @require_logged_in
  def remove_team_member(self, identifier : utils.identifier):
    member = self.user_from_identifier(identifier)
    user = self.user
    team = self.team_for_user(user, ensure_existence = True)
    self.__require_team_admin(team, user)
    return self.__remove_team_member(team, member)

  @api('/team/<team_id>/members/<user_identifier>', method = 'DELETE')
  @require_logged_in
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
    team = self.team_for_user(user, ensure_existence = True)
    return self.__remove_team_member(team, user)
