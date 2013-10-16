# -*- encoding: utf-8 -*-

from bson import ObjectId

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
  def device_view(self, id):
    """Return one user device.
    """
    device = self.database.devices.find_one(
      {
        '_id': ObjectId(id),
        'owner': self.user['_id'],
      },
      #fields = ['_id']
      )

    if device is None:
      self.fail(error.DEVICE_NOT_VALID)
    return self.success(device)

  def _create_device(self, name, user = None):
    """Create a device.
    """
    if regexp.Validator(regexp.Device, error.DEVICE_NOT_VALID)(name) != 0:
      self.fail(error.DEVICE_NOT_VALID)
    user = self.user or user
    assert user is not None
    to_save = {'name': name.strip(), 'owner': user['_id'],}
    id = self.database.devices.insert(to_save)
    assert id is not None
    to_save['passport'] = metalib.generate_passport(
      str(id),
      name,
      user['public_key'],
      conf.INFINIT_AUTHORITY_PATH,
      conf.INFINIT_AUTHORITY_PASSWORD
    )
    self.database.devices.update({"_id": id}, to_save)
    device = self.database.devices.find_one({"_id": id})
    # XXX check unique device ?
    user = self.database.users.find_and_modify({'_id': user['_id']}, {'$addToSet': {'devices': id}})
    return device

  @api('/device/create', method="POST")
  @require_logged_in
  def create_device(self, name):
    device = self._create_device(name)
    assert device is not None
    return self.success({"created_device_id": device['_id'],
                         "passport": device['passport'],
                         "name": device['name']})


  @api('/device/update', method = "POST")
  @require_logged_in
  def update_device(self, id, name):
      """Rename an existing device.
      """
      #regexp.Validator(regexp.Device, error.DEVICE_NOT_VALID)
      #regexp.Validator(regexp.NetworkID, error.DEVICE_ID_NOT_VALID)
      user = self.user
      id = ObjectId(id)
      device = self.database.devices.find_one({'_id': id})
      if device is None:
        self.fail(error.DEVICE_NOT_FOUND)
      if not id in user['devices']:
        self.fail(error.DEVICE_DOESNT_BELONG_TOU_YOU)
      self.database.device.update({'_id': id}, {"$set": {"name": name}})
      return self.success({
          'updated_device_id': str(id),
          'passport': device['passport'],
          'name' : name,
          })

  @api('/device/delete', method = "POST")
  @require_logged_in
  def delete_device(self, id):
    """Delete a device.
    """
    #regexp.Validator(regexp.NotNull, error.DEVICE_ID_NOT_VALID)),
    user = self.user
    id = ObjectId(id)
    device = self.database.devices.find_one({'_id': id})
    if device is None:
      self.fail(error.DEVICE_NOT_FOUND)
    if not id in user.get('devices', []):
      self.fail(error.DEVICE_DOESNT_BELONG_TOU_YOU)
    self.database.devices.remove(id)
    self.database.users.update({'_id': user['_id']}, {'$pull': {'devices': id}})
    return self.success({'deleted_device_id': str(id),})
