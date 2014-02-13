#!/usr/bin/env python3
# -*- python -*-

import bottle
import bson
import calendar
import datetime
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
                           group = None,
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
    if group is not None:
      group = self.__database.groups.find_one({'name': group})
      if group is None:
        self.not_found('group does not exist: %s' % group)
      match = {
        '$and': [
          match,
          {'$or': [
            {'sender_id': {'$in': group['members']}},
            {'recipient_id': {'$in': group['members']}},
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
    print(days)
    ids = set()
    senders = {}
    recipients = {}
    for day in days:
      day['date'] = self.__format_date(day['_id'])
      del day['_id']
      for transaction in day['transactions']:
        sender_id = transaction['sender_id']
        recipient_id = transaction['recipient_id']
        senders.setdefault(sender_id, []).append(transaction)
        recipients.setdefault(recipient_id, []).append(transaction)
        ids.add(sender_id)
        ids.add(recipient_id)
        del transaction['sender_id']
        del transaction['recipient_id']
        transaction['status'] = statuses[transaction['status']]
        transaction['date'] = self.__format_date(transaction['date'])
        transaction['time'] = self.__format_date(transaction['time'])
    for user in self.database.users.aggregate([
        {'$match': {'_id': {'$in': list(ids)}}},
        {'$project': {
          'handle': '$handle',
          'email': '$email',
          'fullname': '$fullname',
          'connected': '$connected',
        }},
    ])['result']:
      for transaction in senders.get(user['_id'], ()):
        transaction['sender'] = user
      for transaction in recipients.get(user['_id'], ()):
        transaction['recipient'] = user
      del user['_id']
    return {
      'result': days,
    }

  @api('/metrics/transactions.html')
  def metrics_transactions_html(self,
                                start : datetime.datetime = None,
                                end : datetime.datetime = None,
                                group = None):
    tpl = self._Meta__mako.get_template('/metrics/transactions.html')
    transactions = self.metrics_transactions(
      start = start, end = end, group = group)['result']
    return tpl.render(transactions = transactions)

  @api('/metrics/waterfall.html')
  def metrics_transactions_html(self):
    tpl = self._Meta__mako.get_template('/metrics/waterfall.html')
    return tpl.render()

  @api('/metrics/transactions/groups', method = 'GET')
  def groups(self):
    groups = self.__database.groups.find()
    return {
      'groups': list(groups),
      }

  @api('/metrics/transactions/groups/<name>', method = 'PUT')
  def group_add(self, name):
    try:
      self.__database.groups.insert(
        {
          'name': name,
          'members': [],
        })
    except pymongo.errors.DuplicateKeyError:
      pass

  @api('/metrics/transactions/groups/<name>', method = 'DELETE')
  def group_remove(self, name):
    res = self.__database.groups.remove({'name': name})
    if res['n'] == 0:
      self.not_found('group does not exist: %s' % name)

  @api('/metrics/transactions/groups/<name>', method = 'GET')
  def group(self, name):
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
  def group_member_add(self, group, user):
    user = self.user_fuzzy(user)
    res = self.__database.groups.find_and_modify(
      query = {'name': group},
      update = {'$addToSet': {'members': user['_id']}},
    )
    if res is None:
      self.not_found('group does not exist: %s' % group)

  @api('/metrics/transactions/groups/<group>/<user>',
       method = 'DELETE')
  def group_member_remove(self, group, user):
    user = self.user_fuzzy(user)
    res = self.__database.groups.find_and_modify(
      query = {'name': group},
      update = {'$pull': {'members': user['_id']}},
    )
    if res is None:
      self.not_found('group does not exist: %s' % group)
