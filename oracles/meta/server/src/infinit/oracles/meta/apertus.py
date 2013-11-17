# -*- encoding: utf-8 -*-

import bson
import uuid
import random
import time
from . import conf, error, regexp
from .utils import api, require_admin, require_logged_in
import elle.log

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
        '_id': str(uid),
        'ip': self.remote_ip,
        'port': port,
        'time': time.time(),
      },
      upsert = True,
    )
    if res['updatedExisting']:
      elle.log.dump("ping from trophonius: %s" % uid)
    else:
      elle.log.log("register trophonius %s" % uid)
    return self.success()

  @api('/apertus/<uid>', method = 'DELETE')
  def apertus_delete(self,
                     uid: uuid.UUID):
    """Unregister a apertus.
    """
    with elle.log.log("unregister trophonius %s" % uid):
      assert isinstance(uid, uuid.UUID)
      self.database.transactions.update({'fallback': str(uid)},
                                        {'$set': {'fallback': None}},
                                        multi = True)
      res = self.database.apertus.remove({"_id": str(uid)})
      return self.success()

  def choose_apertus(self, transaction_id):
    pass

  @api('/apertus/fallback/<id>')
  @require_logged_in
  def apertus_get_fallback(self,
                           id: bson.ObjectId):
    """Return the selected apertus ip/port for a given transaction_id.
    """
    with elle.log.trace("get fallback for transaction %s" % id):
      user = self.user

      transaction = self.transaction(id, user['_id'])

      if transaction.get('fallback') is None:
        elle.log.debug('choosing a random apertus')
        apertus = self.database.apertus.find(fields = ['ip', 'port'])
        if apertus.count() == 0:
          return self.fail(error.NO_APERTUS)
        index = random.randint(0, apertus.count() - 1)
        fallback = apertus[index]
        elle.log.debug('selected fallback: %s' % fallback)
        fallback = '%s:%s' % (fallback['ip'], fallback['port'])
        _transaction = self.database.transactions.find_and_modify(
          {
            '_id': id,
            'fallback': None,
          },
          {
            '$set': {'fallback': fallback},
          },
          new = True,
        )
        if _transaction is None: # Race condition.
          elle.log.warn('transaction has been modified before')
          _transaction = self.database.transactions.find_and_modify(
            {
              '_id': id,
              'fallback': fallback,
            },
            {
              '$set': {'fallback': None},
            },
            new = False,
          )
          fallback = _transaction['fallback']
      else:
        fallback = transaction['fallback']
        elle.log.debug('got fallback from transaction: %s' % fallback)
        _transaction = self.database.transactions.find_and_modify(
          {
            '_id': id,
            'fallback': fallback,
          },
          {
            '$set': {'fallback': None},
          },
          new = True,
        )

      return self.success({'fallback': fallback})

  @api('/apertus')
  def registered_apertus(self):
    apertus = self.database.apertus.find(fields = ['ip', 'port'])
    return self.success({'apertus': ["%s:%s" % (s['ip'], s['port']) for s in apertus]})
