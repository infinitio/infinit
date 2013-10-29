# -*- encoding: utf-8 -*-

import bson
import uuid

from . import conf, error, regexp, metalib
from .utils import api, require_logged_in

class Mixin:

  @api('/devices')
  @require_logged_in
  def devices(self):
    """Return all user's device ids.
    """
    return self.success({'devices': self.user.get('devices', [])})

  @api('/device/<id>/view')
  @require_logged_in
  def device_view(self, id: uuid.UUID):
    """Return one user device.
    """
    id = uuid.UUID(id)
    device = self.database.devices.find_one(
      {
        '_id': id,
        'owner': self.user['_id'],
      },
      #fields = ['_id']
      )

    if device is None:
      self.fail(error.DEVICE_NOT_VALID)
    return self.success(device)

  def _create_device(self,
                     owner,
                     name = None,
                     id = None):
    """Create a device.
    """
    if id is not None:
      assert isinstance(id, uuid.UUID)
    else:
      id = uuid.uuid4()
    if name is None:
      name = str(id)
    print("device name", name)
    if regexp.Validator(regexp.DeviceName, error.DEVICE_NOT_VALID)(name) != 0:
      self.fail(error.DEVICE_NOT_VALID)
    if self.database.devices.find_one({'_id': id}) is not None:
      self.fail(error.DEVICE_ALREADY_REGISTRED)
    to_save = {'name': name.strip(), 'owner': owner['_id'], '_id': id}
    self.database.devices.insert(to_save, upsert = True)
    assert id is not None
    to_save['passport'] = metalib.generate_passport(
      str(id),
      name,
      owner['public_key'],
      conf.INFINIT_AUTHORITY_PATH,
      conf.INFINIT_AUTHORITY_PASSWORD
    )
    self.database.devices.update({"_id": id}, to_save)
    device = self.database.devices.find_one({"_id": id})
    # XXX check unique device ?
    self.database.users.find_and_modify({'_id': owner['_id']}, {'$addToSet': {'devices': id}})
    return device

  @api('/device/create', method="POST")
  @require_logged_in
  def create_device(self, id = None, name = None):
    if id is not None:
      id = uuid.UUID(id)
    device = self._create_device(owner = self.user, id = id, name = name)
    assert device is not None
    return self.success({"_id": device['_id'],
                         "passport": device['passport'],
                         "name": device['name']})

  @api('/device/<id>/<device_id>/connected')
  def is_device_connected(self, id: bson.ObjectId, device_id: uuid.UUID):
    try:
      return self.success({"connected": self._is_connected(id, device_id)})
    except error.Error as e:
      self.fail(*e.args)

  def _is_connected(self, user_id, device_id = None):
    """Get the connection status of a given user.

    user_id -- the id of the user.
    """
    assert isinstance(user_id, bson.ObjectId)
    if device_id is not None:
      assert isinstance(device_id, uuid.UUID)
      user = self._user_by_id(user_id)
      if device_id not in user['devices']:
        raise error.Error(error.DEVICE_DOESNT_BELONG_TOU_YOU)
      return self.database.devices.find_one(
        {
          "_id": device_id,
          "owner": user_id
        },
        fields = ['trophonius']).get('trophonius') is not None
    else:
      return self.database.devices.find(
        {
          "owner": user_id,
          "trophonius": {"$ne": None},
        }).count() > 0

  @api('/device/update', method = "POST")
  @require_logged_in
  def update_device(self, id: uuid.UUID, name):
      """Rename an existing device.
      """
      #regexp.Validator(regexp.Device, error.DEVICE_NOT_VALID)
      #regexp.Validator(regexp.NetworkID, error.DEVICE_ID_NOT_VALID)
      user = self.user
      id = uuid.UUID(id)
      device = self.database.devices.find_one({'_id': id})
      if device is None:
        self.fail(error.DEVICE_NOT_FOUND)
      if not id in user['devices']:
        self.fail(error.DEVICE_DOESNT_BELONG_TOU_YOU)
      self.database.device.update({'_id': id}, {"$set": {"name": name}})
      return self.success({
          '_id': str(id),
          'passport': device['passport'],
          'name' : name,
          })

  @api('/device/delete', method = "POST")
  @require_logged_in
  def delete_device(self, id: uuid.UUID):
    """Delete a device.
    """
    #regexp.Validator(regexp.NotNull, error.DEVICE_ID_NOT_VALID)),
    user = self.user
    id = uuid.UUID(id)
    device = self.database.devices.find_one({'_id': id})
    if device is None:
      self.fail(error.DEVICE_NOT_FOUND)
    if not id in user.get('devices', []):
      self.fail(error.DEVICE_DOESNT_BELONG_TOU_YOU)
    self.database.devices.remove(id)
    self.database.users.update({'_id': user['_id']}, {'$pull': {'devices': id}})
    return self.success({'_id': str(id),})
