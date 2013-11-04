# -*- encoding: utf-8 -*-

import bson
import uuid
import random
from . import conf, error, regexp
from .utils import api, require_admin, require_logged_in

# XXX: Make it generic with trophonius.

class Mixin:

  @api('/apertus/<uid>', method = 'PUT')
  def apertus_put(self,
                  uid: uuid.UUID,
                  port):
    """Register a apertus.
    """
    print("aggregate apertus %s" % uid)
    assert isinstance(uid, uuid.UUID)
    # Upsert is important here  be cause if a apertus crashed and didn't
    # unregister itself, it's important to update the old entry in the database.
    res = self.database.apertus.insert(
      {
        '_id': str(uid),
        'ip': self.remote_ip,
        'port': port,
      },
      upsert = True,
    )
    return self.success()

  @api('/apertus/<uid>', method = 'DELETE')
  def apertus_delete(self,
                     uid: uuid.UUID):
    """Unregister a apertus.
    """
    print("delete apertus %s" % uid)
    assert isinstance(uid, uuid.UUID)
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
    user = self.user
    transaction = self.transaction(id, owner_id = user['_id'])
    if transaction.get('fallback') is not None:
      return self.success({'fallback': transaction.get('fallback')})
    else:
      apertus = self.database.apertus.find(fields = ['ip', 'port'])
      if apertus.count() == 0:
        return self.fail(error.NO_APERTUS)
      index = random.randint(0, apertus.count() - 1)
      fallback = apertus[index]
      fallback = '%s:%s' % (fallback['ip'], fallback['port'])
      self.database.transactions.find_and_modify(
        id, {'$set': {'fallback': fallback}})
      return self.success({'fallback': fallback})

  @api('/apertus/fallbacks')
  def apertus_fallbacks(self):
    apertus = self.database.apertus.find(fields = ['ip', 'port'])
    return self.success({'apertus': ["%s:%s" % (s['ip'], s['port']) for s in apertus]})
