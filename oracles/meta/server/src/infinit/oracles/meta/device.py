# -*- encoding: utf-8 -*-

import bson
import uuid

from . import conf, error, regexp
import papier
from .utils import api, require_logged_in

# We use UUID for typechecking but they are all cast into str represnetation.
# The reason is because the (py)mongo store them as BinData, making them
# impossible to search from mongo shell.
class Mixin:

  @require_logged_in
  @api('/devices')
  def devices(self):
    """Return all user's device ids.
    """
    return self.success({'devices': self.user.get('devices', [])})

  @require_logged_in
  @api('/device/<id>/view')
  def device_view(self,
                  id: uuid.UUID):
    """Return one user device.
    """
    assert isinstance(id, uuid.UUID)
    device = self.database.devices.find_one(
      {
        '_id': str(id),
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
    if regexp.Validator(regexp.DeviceName, error.DEVICE_NOT_VALID)(name) != 0:
      self.fail(error.DEVICE_NOT_VALID)
    if self.database.devices.find_one({'_id': str(id)}) is not None:
      self.fail(error.DEVICE_ALREADY_REGISTRED)
    to_save = {'name': name.strip(), 'owner': owner['_id'], '_id': str(id)}
    self.database.devices.insert(to_save, upsert = True)
    to_save['passport'] = papier.generate_passport(
      str(id),
      name,
      owner['public_key'],
      conf.INFINIT_AUTHORITY_PATH,
      conf.INFINIT_AUTHORITY_PASSWORD
    )
    self.database.devices.update({"_id": str(id)}, to_save)
    device = self.database.devices.find_one({"_id": str(id)})
    # XXX check unique device ?
    self.database.users.find_and_modify({'_id': owner['_id']}, {'$addToSet': {'devices': str(id)}})
    return device

  @require_logged_in
  @api('/device/create', method="POST")
  def create_device(self,
                    id = None,
                    name = None):
    if id is not None:
      assert isinstance(id, uuid.UUID)
    else:
      id = uuid.uuid4()

    device = self._create_device(owner = self.user, id = id, name = name)
    assert device is not None
    return self.success({"_id": device['_id'],
                         "passport": device['passport'],
                         "name": device['name']})

  @api('/device/<id>/<device_id>/connected')
  def is_device_connected(self,
                          id: bson.ObjectId,
                          device_id: uuid.UUID):
    assert isinstance(id, bson.ObjectId)
    assert isinstance(device_id, uuid.UUID)
    try:
      return self.success({"connected": self._is_connected(id, device_id)})
    except error.Error as e:
      self.fail(*e.args)

  def _is_connected(self, user_id, device_id = None):
    """Get the connection status of a given user.

    user_id -- the id of the user.
    """
    assert isinstance(user_id,
                      bson.ObjectId)
    if device_id is not None:
      assert isinstance(device_id, uuid.UUID)
      user = self._user_by_id(user_id)
      if str(device_id) not in user['devices']:
        raise error.Error(error.DEVICE_DOESNT_BELONG_TOU_YOU)
      return self.database.devices.find_one(
        {
          "_id": str(device_id),
          "owner": user_id
        },
        fields = ['trophonius']).get('trophonius') is not None
    else:
      return self.database.devices.find(
        {
          "owner": user_id,
          "trophonius": {"$ne": None},
        }).count() > 0

  @require_logged_in
  @api('/device/update', method = "POST")
  def update_device(self, id: uuid.UUID, name):
    """Rename an existing device.
    """
    assert isinstance(id, uuid.UUID)
    user = self.user
    assert user is not None
    device = self.database.devices.find_one({'_id': str(id)})
    if device is None:
      self.fail(error.DEVICE_NOT_FOUND)
    if not str(id) in user['devices']:
      self.fail(error.DEVICE_DOESNT_BELONG_TOU_YOU)
    self.database.device.update({'_id': str(id)}, {"$set": {"name": name}})
    return self.success({
        '_id': str(id),
        'passport': device['passport'],
        'name' : name,
      })

  @require_logged_in
  @api('/device/delete', method = "POST")
  def delete_device(self,
                    id: uuid.UUID):
    """Delete a device.
    """
    assert isinstance(id, uuid.UUID)
    user = self.user
    assert user is not None
    device = self.database.devices.find_one({'_id': str(id)})
    if device is None:
      self.fail(error.DEVICE_NOT_FOUND)
    if not str(id) in user.get('devices', []):
      self.fail(error.DEVICE_DOESNT_BELONG_TOU_YOU)
    self.database.devices.remove(str(id))
    self.database.users.update({'_id': user['_id']}, {'$pull': {'devices': str(id)}})
    return self.success({'_id': str(id)})
