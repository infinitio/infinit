# -*- encoding: utf-8 -*-

import elle.log
import bson
import uuid
import time

from . import conf, error, regexp
from .utils import api, require_admin, require_logged_in

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Trophonius'

class Mixin:

  @api('/trophonius/<uid>', method = 'PUT')
  def trophonius_put(self,
                     uid: uuid.UUID,
                     port):
    """Register a trophonius.
    """
    assert isinstance(uid, uuid.UUID)
    # Upsert is important here  be cause if a trophonius crashed and didn't
    # unregister itself, it's important to update the old entry in the database.
    res = self.database.trophonius.update(
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

  @api('/trophonius/<uid>', method = 'DELETE')
  def trophonius_delete(self,
                        uid: uuid.UUID):
    """Unregister a trophonius.
    """
    with elle.log.log("unregister trophonius %s" % uid):
      assert isinstance(uid, uuid.UUID)
      elle.log.debug("trophonius %s: unregister all users" % uuid)
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
    with elle.log.trace("trophonius %s: register user (%s)'s device (%s)" %
                        (uid, id, device)):
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
    with elle.log.trace("trophonius %s: unregister user (%s)'s device (%s)" %
                        (uid, id, device)):
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

  @api('/trophonius')
  def registered_trophonius(self):
    trophonius = self.database.trophonius.find(fields = ['ip', 'port'])
    return self.success({'trophonius': ["%s:%s" % (s['ip'], s['port']) for s in trophonius]})
