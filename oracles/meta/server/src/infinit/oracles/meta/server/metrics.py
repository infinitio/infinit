#!/usr/bin/env python3
# -*- python -*-

import bottle
import bson
import bson.json_util
import calendar
import datetime
import json
import os
import time
import pymongo

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

class Mixin:

  @api('/metrics/transactions')
  def metrics_transactions(self,
                           start : datetime.datetime = None,
                           end : datetime.datetime = None):
    if bottle.request.certificate != 'quentin.hocquet@infinit.io':
      self.forbiden()
    if start is None:
      start = datetime.date.today() - datetime.timedelta(7)
    match = {'$gte': calendar.timegm(start.timetuple())}
    if end is not None:
      match['$lte'] = calendar.timegm(end.timetuple())
    match = {'ctime': match}
    transactions = self.database.transactions.aggregate([
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
    ])['result']
    transactions = list(transactions)
    ids = set()
    senders = {}
    recipients = {}
    for transaction in transactions:
      sender_id = transaction['sender_id']
      recipient_id = transaction['recipient_id']
      senders.setdefault(sender_id, []).append(transaction)
      recipients.setdefault(recipient_id, []).append(transaction)
      ids.add(sender_id)
      ids.add(recipient_id)
      del transaction['sender_id']
      del transaction['recipient_id']
      transaction['status'] = statuses[transaction['status']]
      transaction['date'] = datetime.datetime.utcfromtimestamp(
        transaction['date']).isoformat()
      transaction['time'] = datetime.datetime.utcfromtimestamp(
        transaction['time']).isoformat()
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
    return {
      'result': transactions,
    }
