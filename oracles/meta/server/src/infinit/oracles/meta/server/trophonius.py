# -*- encoding: utf-8 -*-

import elle.log
import bottle
import bson
import random
import time
import uuid

from .plugins.response import response
from . import conf, error, regexp
from .utils import api, require_admin, require_logged_in

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Trophonius'

class Mixin:

  @api('/trophonius/<uid>', method = 'PUT')
  def trophonius_put(self,
                     uid: uuid.UUID,
                     port: int,
                     port_client: int,
                     port_client_ssl: int,
                     users: int = 0):
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
        'port_client': port_client,
        'port_client_ssl': port_client_ssl,
        'time': time.time(),
        'users': users,
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

  @api('/trophonius/<uid>/users/<id>/<device>', method = 'PUT')
  @require_logged_in
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
      try:
        self.set_connection_status(user_id = id,
                                   device_id = device,
                                   status = True)
      except error.Error as e:
        return self.fail(*e.args)
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
      try:
        self.set_connection_status(user_id = id,
                                   device_id = device,
                                   status = False)
      except error.Error as e:
        return self.fail(*e.args)
      return self.success()

  # XXX: Debuggin purpose.
  @api('/trophonius/message/user/<user>', method = 'POST')
  def trophonius_message_user(self,
                              user: bson.ObjectId,
                              body):
    type = body.get('notification_type', 208)
    self.notifier.notify_some(notification_type = type,
                              recipient_ids = {user},
                              message = body)
    return self.success()

  @api('/trophonius/message/device/<device>', method = 'POST')
  def trophonius_message_device(self,
                                device: uuid.UUID,
                                message):
    self.notifier.notify_some(notification_type = 208,
                              device_ids = {device},
                              message = {"message": message})
    return self.success()

  @api('/trophoniuses')
  def registered_trophonius(self):
    trophoniuses = self.database.trophonius.find(
      fields = [
        'ip',
        'port',
        'port_client',
        'port_client_ssl',
        'users',
      ])
    return {
      'trophoniuses': list(trophoniuses),
    }

  @api('/trophonius')
  @require_logged_in
  def trophonius_pick(self):
    trophoniuses = self.database.trophonius.find(
      fields = ['ip', 'port_client', 'port_client_ssl'],
      sort = [('users', 1)],
    )
    try:
      trophonius = next(trophoniuses)
      return {
        'host': trophonius['ip'],
        'port': trophonius['port_client'],
        'port_ssl': trophonius['port_client_ssl'],
      }
    except StopIteration:
      response(503, 'no notification server available')
