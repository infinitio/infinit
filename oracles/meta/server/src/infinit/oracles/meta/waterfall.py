#!/usr/bin/env python3
# -*- python -*-

import bottle
import bson
import calendar
import datetime
import time
import pymongo

import elle.log
from infinit.oracles.meta.utils import api, require_logged_in

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Waterfall'

statuses = {
  0: ("created", 0x8080FF),
  1: ("initialized", 0x8080FF),
  2: ("accepted", 0x80FFFF),
  4: ("finished", 0x80FF90),
  5: ("rejected", 0xFFFF80),
  6: ("canceled", 0xFFFF80),
  7: ("failed", 0xFF8080)
}

def user_to_json(user):
  keys = ['_id', 'email']
  return {k: user[k] for k in keys}

class Mixin:

  def __waterfall(self, filter = {}):
    today = datetime.date.today()
    start = today - datetime.timedelta(7)
    start_ts = calendar.timegm(start.timetuple())
    match = {'ctime': {'$gte': start_ts}}
    match.update(filter)
    res = list(self.database.transactions.aggregate([
      # Keep only recent transaction
      {'$match': match},
      # Count days
      {'$project': {
        'ctime': {'$divide': [{'$subtract': ['$ctime', start_ts]}, 60 * 60 * 24]},
        'transaction': {
          'sender': '$sender_fullname',
          'recipient': '$recipient_fullname',
          'sender_id': '$sender_id',
          'recipient_id': '$recipient_id',
          'status': '$status',
        }
      }},
      # Regroup by days
      {'$group': {
        '_id': {'$subtract': ['$ctime', {'$mod': ['$ctime', 1]}]},
        'count': {'$sum': 1},
        'transactions': {'$push': '$transaction'},
      }},
      #
      {'$sort': {'_id': 1}},
    ])['result'])
    # Table
    yield '<table>\n'
    # Header
    yield '  <tr>\n'
    for day in res:
      date = (start + datetime.timedelta(day['_id']))
      yield '    <th>%s (%s)</th>\n' % (date, day['count'])
    yield '  </tr>\n'
    # Days
    def combine(days):
      def next_or_none(i):
        try:
          return next(i)
        except StopIteration:
          return None
      iterators = [iter(day['transactions']) for day in days]
      while True:
        res = [next_or_none(i) for i in iterators]
        if not any(res):
          break
        yield res
    for line in combine(res):
      yield '  <tr>\n'
      for t in line:
        if t is not None:
          fmt = {
            'color': statuses[t['status']][1],
            'sender': t['sender'],
            'recipient': t['recipient'],
            'sender_id': t['sender_id'],
            'recipient_id': t['recipient_id'],
          }
          yield '    <td style="background:%(color)x">\n' % fmt
          yield '      <a href="/waterfall/users/%(sender_id)s">%(sender)s</a>\n' % fmt
          yield '      to\n'
          yield '      <a href="/waterfall/users/%(recipient_id)s">%(recipient)s</a>\n' % fmt
          yield '    </td>\n' % fmt
        else:
          yield '    <td/>\n'
      yield '  </tr>\n'
    # /Table
    yield '</table>\n'

  @api('/waterfall')
  def waterfall(self):
    return self.__waterfall()

  @api('/waterfall/users/<user>')
  def waterfall_user(self, user):
    return self.__waterfall({'$or': [
      {'sender_id': bson.ObjectId(user)},
      {'recipient_id': bson.ObjectId(user)},
    ]})
