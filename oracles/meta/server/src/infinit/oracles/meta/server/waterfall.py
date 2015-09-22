#!/usr/bin/env python3
# -*- python -*-

import bottle
import bson
import calendar
import datetime
import json
import pymongo
import pymongo.errors

import elle.log

from infinit.oracles.meta.server.utils import api
from infinit.oracles.meta.server.utils import json_value
from infinit.oracles.meta.server.utils import utf8_string
from infinit.oracles.meta.server.utils import require_admin
import infinit.oracles.transaction
from itertools import chain

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Metrics'

statuses_back = dict(infinit.oracles.transaction.statuses)
statuses = dict((value, key) for key, value in statuses_back.items())

def develop(collections):
  for collection in collections:
    for element in collection:
      yield element

def join(collection, foreign, joined, project = None):
  related_ids = set()
  store = {}
  for element in collection:
    for related_id, container, index in foreign(element):
      related_ids.add(related_id)
      store.setdefault(related_id, []).append((container, index))
  pipeline = [{'$match': {'_id': {'$in': list(related_ids)}}}]
  if project is not None:
    pipeline.append({'$project': project})
  for related in joined.aggregate(pipeline)['result']:
    for container, index in store[related['_id']]:
      container[index] = related

class Waterfall:

  def __init__(self):
    self.__database = self.database.waterfall
    self.__database.groups.ensure_index([("name", 1)], unique = True)

  def __format_date(self, date):
    return datetime.datetime.utcfromtimestamp(date).isoformat()

  def waterfall_transactions(self,
                             start : datetime.datetime = None,
                             end : datetime.datetime = None,
                             groups = [],
                             users = [],
                             status = None):
    if start is None:
      start = datetime.date.today() - datetime.timedelta(7)
    match = {'$gte': calendar.timegm(start.timetuple())}
    if end is not None:
      match['$lte'] = calendar.timegm(end.timetuple())
    match = {'ctime': match}
    if status is not None:
      status_i = statuses_back.get(status, None)
      if status_i is None:
        self.bad_request('invalid status: %s' % status)
      match['status'] = status_i
    if any((groups, users)):
      if groups:
        groups = self.__database.groups.find({'name': {'$in': groups}})
        groups = develop(group['members'] for group in groups)
        groups = list(groups)
      users = list(chain(groups, (bson.ObjectId(u) for u in users)))
      match = {
        '$and': [
          match,
          {'$or': [
            {'sender_id': {'$in': users}},
            {'recipient_id': {'$in': users}},
          ]}
        ]}
    days = self.database.transactions.aggregate([
      # Keep only recent transaction
      {'$match': match},
      {'$sort': {'ctime': -1}},
      # Count days
      {'$project': {
        'time': '$ctime',
        'date': {'$subtract':
                 ['$ctime', {'$mod': ['$ctime', 60 * 60 * 24]}]},
        'sender_id': '$sender_id',
        'recipient_id': '$recipient_id',
        'status': '$status',
        'size': '$total_size',
        'files': '$files',
      }},
      # Group by day
      {'$group': {
        '_id': '$date',
        'count': {'$sum': 1},
        'size': {'$sum': '$size'},
        'transactions': {'$push': {
          'time': '$time',
          'date': '$date',
          'sender_id': '$sender_id',
          'recipient_id': '$recipient_id',
          'status': '$status',
          'size': '$size',
          'files': '$files',
        }},
      }},
    ])['result']
    days = list(days)
    def foreign(day):
      day['date'] = self.__format_date(day['_id'])
      del day['_id']
      for transaction in day['transactions']:
        yield transaction['sender_id'], transaction, 'sender'
        yield transaction['recipient_id'], transaction, 'recipient'
        del transaction['sender_id']
        del transaction['recipient_id']
        transaction['status'] = statuses[transaction['status']]
        transaction['date'] = self.__format_date(transaction['date'])
        transaction['time'] = self.__format_date(transaction['time'])
    join(days, foreign, self.database.users,
         project = {
            'handle': '$handle',
            'email': '$email',
            'fullname': '$fullname',
          })
    return days

  @api('/waterfall/transactions<html:re:(\\.html)?>')
  @require_admin
  def waterfall_transactions_api(self,
                                 html,
                                 start : datetime.datetime = None,
                                 end : datetime.datetime = None,
                                 groups : json_value = [],
                                 users : json_value = [],
                                 status = None,):
    data = self.waterfall_transactions(start = start,
                                       end = end,
                                       groups = groups,
                                       users = users,
                                       status = status)
    if html:
      tpl = '/waterfall/transactions.html'
      tpl = self._Meta__mako.get_template(tpl)
      host = bottle.request.environ['HTTP_HOST']
      return tpl.render(transaction_days = data, http_host = host)
    else:
      return {
        'result': data,
      }

  @api('/waterfall/groups.html')
  @require_admin
  def waterfall_groups_html(self):
    tpl = self._Meta__mako.get_template('/waterfall/groups.html')
    groups = self.groups()['groups']
    return tpl.render(groups = groups,
                      http_host = bottle.request.environ['HTTP_HOST'])

  @api('/waterfall/manage_groups.html')
  @require_admin
  def waterfall_manage_groups_html(self):
    tpl = self._Meta__mako.get_template(
      '/waterfall/manage_groups.html')
    return tpl.render(root = '..', title = 'groups')

  @api('/waterfall')
  @require_admin
  def waterfall_transactions_html(self, users : json_value = []):
    tpl = self._Meta__mako.get_template('/waterfall/waterfall.html')
    return tpl.render(root = '..', title = 'waterfall')

  @api('/waterfall/users.html')
  def user_search_html(self,
                       search = None,
                       limit : int = 5,
                       skip : int = 0):
    tpl = self._Meta__mako.get_template('/waterfall/user_search.html')
    users = self.users(search = search,
                       limit = limit, skip = skip)['result']
    return tpl.render(root = '..',
                      http_host = bottle.request.environ['HTTP_HOST'],
                      title = 'user_search', users = users)

  @api('/waterfall/transactions/groups', method = 'GET')
  @require_admin
  def groups(self):
    groups = list(self.__database.groups.find())
    def foreign(group):
      for i, related_id in enumerate(group['members']):
        yield related_id, group['members'], i
    join(groups, foreign, self.database.users,
         project = {
            'handle': '$handle',
            'email': '$email',
            'fullname': '$fullname',
          })
    return {
      'groups': list(groups),
      }

  @api('/waterfall/transactions/groups/<name>', method = 'PUT')
  @require_admin
  def group_add(self, name: utf8_string):
    try:
      self.__database.groups.insert(
        {
          'name': name,
          'members': [],
        })
    except pymongo.errors.DuplicateKeyError:
      pass

  @api('/waterfall/transactions/groups/<name>', method = 'DELETE')
  @require_admin
  def group_remove(self, name: utf8_string):
    res = self.__database.groups.remove({'name': name})
    if res['n'] == 0:
      self.not_found('group does not exist: %s' % name)

  @api('/waterfall/transactions/groups/<name>', method = 'GET')
  @require_admin
  def group(self, name: utf8_string):
    group = self.__database.groups.find_one({'name': name})
    if group is None:
      self.not_found('group does not exist: %s' % name)
    return group

  def user_fuzzy(self, user):
    res = None
    try:
      i = bson.ObjectId(user)
      res = self.user_by_id(i, ensure_existence = False)
    except bson.errors.InvalidId:
      pass
    if res is None:
      res = self.user_by_email(user, ensure_existence = False)
      if res is None:
        res = self.user_by_handle(user, ensure_existence = False)
        if res is None:
          self.not_found('user does not exist: %s' % user)
    return res

  @api('/waterfall/transactions/groups/<group>/<user>',
       method = 'PUT')
  @require_admin
  def group_member_add(self, group: utf8_string, user):
    user = self.user_fuzzy(user)
    res = self.__database.groups.find_and_modify(
      query = {'name': group},
      update = {'$addToSet': {'members': user['_id']}},
    )
    if res is None:
      self.not_found('group does not exist: %s' % group)

  @api('/waterfall/transactions/groups/<group>/<user>',
       method = 'DELETE')
  @require_admin
  def group_member_remove(self, group: utf8_string, user):
    user = self.user_fuzzy(user)
    res = self.__database.groups.find_and_modify(
      query = {'name': group},
      update = {'$pull': {'members': user['_id']}},
    )
    if res is None:
      self.not_found('group does not exist: %s' % group)
