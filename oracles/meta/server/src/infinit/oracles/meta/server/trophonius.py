# -*- encoding: utf-8 -*-

import elle.log
import bottle
import bson
import random
import time
import uuid

from .plugins.response import response
from . import conf, error, regexp, notifier
from .utils import api, require_admin, require_logged_in
from collections import OrderedDict

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Trophonius'

class Mixin:

  @api('/trophonius/<uid>', method = 'PUT')
  def trophonius_put(self,
                     uid: uuid.UUID,
                     port: int,
                     port_client: int,
                     port_client_ssl: int,
                     hostname: str = None,
                     users: int = 0,
                     version = None,
                     zone = None,
                     shutting_down = False,
  ):
    """Register a trophonius.
    """
    # Upsert is important here  be cause if a trophonius crashed and didn't
    # unregister itself, it's important to update the old entry in the database.
    res = self.database.trophonius.update(
      {
        '_id': str(uid),
      },
      {
        '_id': str(uid),
        'hostname': hostname,
        'ip': self.remote_ip,
        'port': port,
        'port_client': port_client,
        'port_client_ssl': port_client_ssl,
        'time': time.time(),
        'users': users,
        'version': version,
        'zone': zone,
        'shutting_down': shutting_down,
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
                               device: uuid.UUID,
                               version = None,
                               os = None):
    with elle.log.trace('trophonius %s: register user %s device %s' %
                        (uid, id, device)):
      assert isinstance(uid, uuid.UUID)
      assert isinstance(device, uuid.UUID)
      version = version and OrderedDict(sorted(version.items()))
      self.database.devices.update(
        {
          'id': str(device),
          'owner': id,
        },
        {
          '$set':
          {
            'trophonius': str(uid),
            'version': version,
            'os': os,
          }
        }
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

  @property
  def __trophonius_query(self):
    return {
      'zone': self._Meta__zone,
    }


  @api('/trophoniuses')
  def registered_trophonius(self):
    trophoniuses = self.database.trophonius.find(
      self.__trophonius_query,
      fields = [
        'hostname',
        'ip',
        'port',
        'port_client',
        'port_client_ssl',
        'shutting_down',
        'users',
        'version',
      ])
    trophoniuses = list(trophoniuses)
    def format_version(version):
      if version is None:
        return None
      return '%s.%s.%s' % (version['major'],
                           version['minor'],
                           version['subminor'])

    clients = self.database.devices.aggregate([
      {'$match': {'trophonius': {'$in': [t['_id'] for t in trophoniuses]}}},
      {
        '$group':
        {
          '_id': {'os': '$os', 'version': '$version'},
          'count': {'$sum': 1},
        }
      },
    ])['result']

    versions = {}
    oses = {}

    for client in clients:
      version = format_version(client['_id'].get('version', None))
      versions.setdefault(version, 0)
      versions[version] += client['count']
      os = client['_id'].get('os', None)
      # No null keys in JSON, sadly.
      if os is None:
        os = 'unknown'
      oses.setdefault(os, {})
      oses[os].setdefault('total', 0)
      oses[os].setdefault(version, 0)
      oses[os]['total'] += client['count']
      oses[os][version] += client['count']
    return {
      'trophoniuses': trophoniuses,
      'users': sum(tropho['users'] for tropho in trophoniuses),
      'versions': versions,
      'oses': oses,
    }

  @api('/trophonius')
  @require_logged_in
  def trophonius_pick(self):
    query = self.__trophonius_query
    query.update({
      'shutting_down': {'$ne': True}, # False or absent
      'time': {'$gt': time.time() - self.trophonius_expiration_time},
    })
    trophoniuses = self.database.trophonius.find(
      query,
      fields = ['hostname', 'port_client', 'port_client_ssl'],
      sort = [('users', 1)],
    )
    try:
      trophonius = next(trophoniuses)
      return {
        'host': trophonius['hostname'],
        'port': trophonius['port_client'],
        'port_ssl': trophonius['port_client_ssl'],
      }
    except StopIteration:
      response(503, 'no notification server available')

  def trophonius_clean(self):
    # Trophonius.
    tropho = self.database.trophonius.remove(
      {"$or": [{"time": {"$lt": time.time() - self.trophonius_expiration_time}},
               {"time": {"$exists": False}}]},
      multi = True)
    return tropho['n']
