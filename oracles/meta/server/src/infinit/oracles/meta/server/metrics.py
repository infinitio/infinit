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

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Metrics'

statuses = {
  0: ("created"),
  1: ("initialized"),
  2: ("accepted"),
  4: ("finished"),
  5: ("rejected"),
  6: ("canceled"),
  7: ("failed"),
}

statuses_back = dict((value, key) for key, value in statuses.items())

def utf8_string(s):
  return s.encode('latin-1').decode('utf-8')

def json_value(s):
  return json.loads(utf8_string(s))

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

class Metrics:

  def __init__(self):
    self.__database = self.database.waterfall
    self.__database.groups.ensure_index([("name", 1)], unique = True)

  def __format_date(self, date):
    return datetime.datetime.utcfromtimestamp(date).isoformat()

  @api('/metrics/transactions')
  def metrics_transactions(self,
                           start : datetime.datetime = None,
                           end : datetime.datetime = None,
                           groups : json_value = None,
                           status = None):
    # if bottle.request.certificate != 'quentin.hocquet@infinit.io':
    #   self.forbiden()
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
    if groups is not None:
      groups = self.__database.groups.find({'name': {'$in': groups}})
      members = sum((group['members'] for group in groups), [])
      match = {
        '$and': [
          match,
          {'$or': [
            {'sender_id': {'$in': members}},
            {'recipient_id': {'$in': members}},
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
      del day['_id']
      for transaction in day['transactions']:
        yield transaction['sender_id'], transaction, 'sender_id'
        yield transaction['recipient_id'], transaction, 'recipient_id'
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
            'connected': '$connected',
          })
    return {
      'result': days,
    }

  @api('/metrics/transactions.html')
  def metrics_transactions_html(self,
                                start : datetime.datetime = None,
                                end : datetime.datetime = None,
                                group = None,
                                status = None):
    tpl = self._Meta__mako.get_template('/metrics/transactions.html')
    transaction_days = self.metrics_transactions(
      start = start, end = end,
      group = group, status = status)['result']
    return tpl.render(transaction_days = transaction_days,
                      http_host = bottle.request.environ['HTTP_HOST'])

  @api('/metrics/waterfall.html')
  def metrics_transactions_html(self):
    tpl = self._Meta__mako.get_template('/metrics/waterfall.html')
    return tpl.render(root = '..', title = 'waterfall')

  @api('/metrics/transactions/groups', method = 'GET')
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
            'connected': '$connected',
          })
    return {
      'groups': list(groups),
      }

  @api('/metrics/transactions/groups/<name>', method = 'PUT')
  def group_add(self, name: utf8_string):
    try:
      self.__database.groups.insert(
        {
          'name': name,
          'members': [],
        })
    except pymongo.errors.DuplicateKeyError:
      pass

  @api('/metrics/transactions/groups/<name>', method = 'DELETE')
  def group_remove(self, name: utf8_string):
    res = self.__database.groups.remove({'name': name})
    if res['n'] == 0:
      self.not_found('group does not exist: %s' % name)

  @api('/metrics/transactions/groups/<name>', method = 'GET')
  def group(self, name: utf8_string):
    group = self.__database.groups.find_one({'name': name})
    if group is None:
      self.not_found('group does not exist: %s' % name)
    return group

  def user_fuzzy(self, user):
    res = None
    try:
      i = bson.ObjectId(user),
      res = self._user_by_id(i, ensure_existence = False)
    except bson.errors.InvalidId:
      pass
    if res is None:
      res = self.user_by_email(user, ensure_existence = False)
      if res is None:
        res = self.user_by_handle(user, ensure_existence = False)
        if res is None:
          self.not_found('user does not exist: %s' % user)
    return res

  @api('/metrics/transactions/groups/<group>/<user>',
       method = 'PUT')
  def group_member_add(self, group: utf8_string, user):
    user = self.user_fuzzy(user)
    res = self.__database.groups.find_and_modify(
      query = {'name': group},
      update = {'$addToSet': {'members': user['_id']}},
    )
    if res is None:
      self.not_found('group does not exist: %s' % group)

  @api('/metrics/transactions/groups/<group>/<user>',
       method = 'DELETE')
  def group_member_remove(self, group: utf8_string, user):
    user = self.user_fuzzy(user)
    res = self.__database.groups.find_and_modify(
      query = {'name': group},
      update = {'$pull': {'members': user['_id']}},
    )
    if res is None:
      self.not_found('group does not exist: %s' % group)
