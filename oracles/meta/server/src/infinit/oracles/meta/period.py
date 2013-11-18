#!/usr/bin/env python3
# -*- python -*-

import bottle
import bson
import datetime
import time
import pymongo

import elle.log
from . import conf, mail, error, notifier, transaction_status
from .utils import api, require_admin, hash_pasword
from .plugins.jsongo import jsonify
import infinit.oracles.meta.version

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Period'


class User():
  def __init__(self, parent, data):
    if isinstance(data, bson.ObjectId):
      data = parent._user_by_id(data)
    self.json = data
    self.json['trs'] = parent.period_transactions(self.json['_id'], count_only = True)
    self.status = self.json['register_status']

  def __lt__(self, lhs):
    return self.json['trs'] < lhs.json['trs']

  def __str__(self):
    return """<a rel="tooltip" title="%(register_status)s" href=/period/user/%(_id)s>
              %(email)s %(trs)s</a>""" % self.json

class Transaction():

  statuses = {
    0: ("created", 0xA5A2FA),
    1: ("initialized", 0xA5A2FA),
    2: ("accepted", 0x5A55FA),
    4: ("finished", 0x4FFF30),
    5: ("rejected", 0xCCAF47),
    6: ("canceled", 0xCCAF47),
    7: ("failed", 0xEE1717)
  }

  def __lt__(self, lhs):
    return self.json['ctime'] < lhs.json['ctime']

  def __init__(self, parent, data, detailed = False):
    if isinstance(data, bson.ObjectId):
      data = parent.transaction(data)
    self.parent = parent
    self.json = data
    self.json['status'], self.json['color'] = Transaction.statuses[self.json['status']]
    self.json['sender_id'] = User(parent, parent._user_by_id(self.json['sender_id'], ensure_existence = False))
    self.json['recipient_id'] = User(parent, parent._user_by_id(self.json['recipient_id'], ensure_existence = False))
    self.json['ctime'] = datetime.datetime.fromtimestamp(self.json['ctime'])
    self.json['files'] = detailed and ': %s' % self.json['files'] or ''

  def __str__(self):
    return """<div style=background:%(color)x title="%(_id)s:%(status)s">
              %(sender_id)s -> %(recipient_id)s %(files)s</div>""" % self.json

class Table():

  def __init__(self):
    self.__data = None
    self.rows = []

  def __enter__(self):
    self.__data = "<table> <tr>"
    return self

  def __exit__(self, *a, **kw):
    self.__data += "</table> </tr>"

  class Row():
    def __init__(self):
      self.__data = None

    def __enter__(self):
      self.__data = "<li>"
      return self

    def __exit__(self, *a, **kw):
      self.__data += "</il>"

    def put(self, data):
      self.__data += str(data)

    def to_string(self):
      return self.__data

  def append(self, data):
    if isinstance(data, list):
      for entry in data:
        r = Table.Row()
        with r as row:
          row.put(entry)
        self.rows.append(r)
    else:
      r = Table.Row()
      with r as row:
        row.put(data)
      self.rows.append(r)

  def to_string(self):
    for row in self.rows:
      self.__data += row.to_string()
    return self.__data

class Page():
  def __init__(self, body):
    self.body = """<html> <body> <div> <h3> <a href="/period"> back to period </a> </h3> %s</div> </body> </html>""" % body

  def __str__(self):
    return self.body


def user_to_json(user):
  keys = ['_id', 'email']
  return {k: user[k] for k in keys}


class Mixin:

  def _transactions_per_day(self):
    import datetime
    import time
    from math import floor
    now = datetime.datetime.now()
    min = time.mktime(datetime.datetime(now.year, now.month, now.day).timetuple())
    seconds_per_day = 3600 * 24
    count = 5
    MIN = min - count * seconds_per_day
    ts = list(
      t for t in self.database.transactions.find({'ctime': {'$gt': MIN}})
    )
    counts = [0] * (count + 1)
    users = {}
    for t in ts:
      idx = datetime.datetime.fromtimestamp(t['ctime'])
      idx = "%s-%s-%s" % (idx.day, idx.month, idx.year)
      users.setdefault(idx, [])
      users[idx].append(Transaction(self, t))
    return users

  def period_transactions(self, user, count_only = False):
    query = {"$or": [{'sender_id': bson.ObjectId(user)},
                     {'recipient_id': bson.ObjectId(user)}]}
    if count_only:
      return self.database.transactions.find(query).count()
    return list(self.database.transactions.find(query))

  @api('/period')
  def period(self):
    tpd = self._transactions_per_day()
    out = "<table> <tr>"
    for day in tpd:
      trs = tpd[day]
      out += '<td align=center>%s: (%s)</td>' % (day, len(trs))
    out += '</tr><tr>'
    for day in tpd:
      out += '<td>'
      trs = tpd[day]
      for t in trs:
        out += str(t)
        out += "<br>"
      out += '</td>'
    out += "</tr></table>"
    return str(Page(out))

  @api('/period/users')
  def period_users(self):
    t = Table()
    users = list()
    with t as table:
      for user in self.database.users.find({'register_status': 'ok'}):
        users.append(User(self, user))
      users.sort(reverse = True)
      table.append(users)
    return str(Page(t.to_string()))

  @api('/period/user/<user>')
  def period_user(self, user):
    t = Table()
    with t as table:
      trs = list()
      for tr in self.database.transactions.find({"$or": [{'sender_id': bson.ObjectId(user)},
                                                    {'recipient_id': bson.ObjectId(user)}]},
                                           sort = [("ctime", pymongo.DESCENDING)],
                                           limit = 40):
        trs.append(Transaction(self, tr))
      trs.sort()
      table.append(trs)
    return str(Page(t.to_string()))
