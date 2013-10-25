# -*- encoding: utf-8 -*-

from bson import ObjectId

from . import conf, error, regexp, metalib
from .utils import api, require_logged_in
from uuid import UUID

class Mixin:

  @api('/trophonius/<uid>', method = 'PUT')
  def trophonius_put(self, uid, port):
    """Register a trophonius.
    """
    uid = UUID(uid)
    self.database.trophonius.insert(
      {
        '_id': uid,
        'ip': self.remote_ip,
        'port': port,
      }
    )
    return self.success()

  @api('/trophonius/<uid>', method = 'DELETE')
  def trophonius_delete(self, uid: UUID):
    """Unregister a trophonius.
    """
    assert isinstance(uid, UUID)
    self.database.devices.update({'trophonius': uid},
                                 {'$set': {'trophonius': None}},
                                 multi = True)
    self.database.trophonius.remove({"_id": uid})
    return self.success()

  @api('/trophonius/<uid>/users/<id>/<device>', method = 'PUT')
  def trophonius_register_user(self, uid, id: ObjectId, device: UUID):
    uid = UUID(uid)
    assert isinstance(device, UUID)

    self.database.devices.update(
      {
        '_id': device,
        'owner': id,
      },
      {'$set': {'trophonius': uid}}
    )
    self.set_connection_status(id, device, True)
    return self.success()

  @api('/trophonius/<uid>/users/<id>/<device>', method = 'DELETE')
  def trophonius_unregister_user(self, uid, id: ObjectId, device: UUID):
    uid = UUID(uid)
    assert isinstance(device, UUID)
    self.database.devices.update(
      {
        '_id': device,
        'owner': id,
        'trophonius': uid,
      },
      {'$set': {'trophonius': None}}
    )
    self.set_connection_status(id, device, False)
    return self.success()
