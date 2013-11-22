# -*- encoding: utf-8 -*-

import bson
import uuid
import time
from . import conf, error, regexp
from .utils import api, require_admin, require_logged_in
import elle.log
import pymongo

# XXX: Make it generic with trophonius.
ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Apertus'

class Mixin:

  @api('/apertus/<uid>', method = 'PUT')
  def apertus_put(self,
                  uid: uuid.UUID,
                  port):
    """Register a apertus.
    """
    assert isinstance(uid, uuid.UUID)
    # Upsert is important here  be cause if a apertus crashed and didn't
    # unregister itself, it's important to update the old entry in the database.
    res = self.database.apertus.update(
      {
        '_id': str(uid),
      },
      {
        '$set': {
        'ip': self.remote_ip,
        'port': port,
        'time': time.time(),

        }
      },
      upsert = True,
    )
    if res['updatedExisting']:
      elle.log.dump("ping from apertus: %s" % uid)
    else:
      elle.log.log("register apertus %s" % uid)
    return self.success()

  @api('/apertus/<uid>', method = 'DELETE')
  def apertus_delete(self,
                     uid: uuid.UUID):
    """Unregister a apertus.
    """
    with elle.log.log("unregister apertus %s" % uid):
      assert isinstance(uid, uuid.UUID)
      self.database.transactions.update({'fallback': str(uid)},
                                        {'$set': {'fallback': None}},
                                        multi = True)
      res = self.database.apertus.remove({"_id": str(uid)})
      return self.success()

  @api('/apertus/<uid>/bandwidth', method = 'POST')
  def apertus_update_bandwidth(self,
                               uid: uuid.UUID,
                               bandwidth,
                               number_of_transfers
                               ):
    """Update current bandwidth.
    """
    with elle.log.trace("update bandwidth %s" % uid):
      assert isinstance(uid, uuid.UUID)
      res = self.database.apertus.find_and_modify(
        {
          '_id': str(uid),
        },
        {
          '$set': {
            'load': bandwidth,
            'number_of_transfers': number_of_transfers,
          }
        })
      return self.success()

  def choose_apertus(self):
    apertus = self.database.apertus.find(
        {
          "ip": { "$ne": None },
          "port": { "$ne": None },
          "load": { "$ne": None },
        }
      )
    if apertus.count() == 0:
      return self.fail(error.NO_APERTUS)
    apertus = apertus.sort([("load", 1)])
    fallback = apertus[0]
    elle.log.debug('selected fallback: %s' % fallback)
    return '%s:%s' % (fallback['ip'], fallback['port'])

  @api('/apertus/fallback/<id>')
  @require_logged_in
  def apertus_get_fallback(self,
                           id: bson.ObjectId):
    """Return the selected apertus ip/port for a given transaction_id.
    """
    with elle.log.trace("get fallback for transaction %s" % id):
      user = self.user
      fallback = self.choose_apertus()
      transaction = self.database.transactions.find_and_modify(
        {
          '_id': id,
          'fallback': None,
        },
        {
          '$set': {'fallback': fallback},
        },
        new = True,
      )
      if transaction is None:
        fallback = self.database.transactions.find_and_modify({'_id': id}, {'$set': {'fallback': None}}, new = False, fields = ['fallback'])['fallback']
      assert fallback is not None
      return self.success({'fallback': fallback})

  apertus_fields = ['ip', 'port', 'load', 'number_of_transfers']

  @api('/apertus')
  def registered_apertus(self):
    result = {}
    db = self.database
    for apertus in db.apertus.find(fields = self.apertus_fields):
      _id = apertus['_id']
      del apertus['_id']
      result[_id] = apertus
    return self.success({'apertus': result})

  @api('/apertus/<uid>', method = 'GET')
  def apertus_get(self,
                  uid: uuid.UUID):
    db = self.database
    apertus = db.apertus.find_one({'_id': str(uid)},
                                  fields = self.apertus_fields)
    if apertus is None:
      self.not_found()
    del apertus['_id']
    return apertus
