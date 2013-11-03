# -*- encoding: utf-8 -*-

import bson
import uuid

from . import conf, error, regexp
from .utils import api, require_admin, require_logged_in

class Mixin:

  @api('/trophonius/<uid>', method = 'PUT')
  def trophonius_put(self,
                     uid: uuid.UUID,
                     port):
    """Register a trophonius.
    """
    print("aggregate trophonius %s" % uid)
    assert isinstance(uid, uuid.UUID)
    # Upsert is important here  be cause if a trophonius crashed and didn't
    # unregister itself, it's important to update the old entry in the database.
    res = self.database.trophonius.insert(
      {
        '_id': str(uid),
        'ip': self.remote_ip,
        'port': port,
      },
      upsert = True,
    )
    return self.success()

  @api('/trophonius/<uid>', method = 'DELETE')
  def trophonius_delete(self,
                        uid: uuid.UUID):
    """Unregister a trophonius.
    """
    print("delete trophonius %s" % uid)
    assert isinstance(uid, uuid.UUID)
    self.database.devices.update({'trophonius': str(uid)},
                                 {'$set': {'trophonius': None}},
                                 multi = True)
    res = self.database.trophonius.remove({"_id": str(uid)})
    return self.success()

  @require_logged_in
  @api('/trophonius/<uid>/users/<id>/<device>', method = 'PUT')
  def trophonius_register_user(self,
                               uid: uuid.UUID,
                               id: bson.ObjectId,
                               device: uuid.UUID):
    print("register user (%s)'s device (%s) to trophonius %s" %
          (id, device, uid))
    assert isinstance(uid, uuid.UUID)
    assert isinstance(device, uuid.UUID)
    self.database.devices.update(
      {
        'id': str(device),
        'owner': id,
      },
      {'$set': {'trophonius': str(uid)}}
    )
    self.set_connection_status(id, device, True)
    return self.success()

  @api('/trophonius/<uid>/users/<id>/<device>', method = 'DELETE')
  def trophonius_unregister_user(self,
                                 uid: uuid.UUID,
                                 id: bson.ObjectId,
                                 device: uuid.UUID):
    print("unregister user (%s)'s device (%s) from trophonius %s" %
          (id, device, uid))
    assert isinstance(uid, uuid.UUID)
    assert isinstance(device, uuid.UUID)
    self.database.devices.update(
      {
        'id': str(device),
        'owner': id,
        'trophonius': str(uid),
      },
      {'$set': {'trophonius': None}}
    )
    self.set_connection_status(id, device, False)
    return self.success()

  # XXX: Debuggin purpose.
  @api('/trophonius/message/user/<user>', method = 'POST')
  def trophonius_message_user(self,
                              user: bson.ObjectId,
                              message):
    self.notifier.notify_some(notification_type = 208,
                              recipient_ids = {user},
                              message = {"message": message})
    return self.success()

  @api('/trophonius/message/device/<device>', method = 'POST')
  def trophonius_message_device(self,
                                device: uuid.UUID,
                                message):
    self.notifier.notify_some(notification_type = 208,
                              device_ids = {device},
                              message = {"message": message})
    return self.success()
