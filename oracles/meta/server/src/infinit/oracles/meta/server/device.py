# -*- encoding: utf-8 -*-

import bson
import copy
import uuid

from . import conf, error, regexp
from .utils import api, require_logged_in

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Device'

# We use UUID for typechecking but they are all cast into str represnetation.
# The reason is because the (py)mongo store them as BinData, making them
# impossible to search from mongo shell.
class Mixin:

  def device(self,
             id,
             owner = None,
             ensure_existence = True,
             **kwargs):
    if isinstance(id, uuid.UUID):
      id = str(id)
    query = {'id': id}
    if owner is not None:
      assert isinstance(owner, bson.ObjectId)
      query['owner'] = owner
    device = self.database.devices.find_one(query, **kwargs)
    if ensure_existence and device is None:
      raise error.Error(error.DEVICE_NOT_FOUND)
    return device

  def remove_devices(self, user):
    self.database.devices.remove({"owner": user['_id']})

  @api('/devices')
  @require_logged_in
  def devices(self):
    """Return all user's device ids.
    """
    return self.success({'devices': self.user.get('devices', [])})

  @api('/device/<id>/view')
  @require_logged_in
  def device_view(self,
                  id: uuid.UUID):
    """Return one user device.
    """
    assert isinstance(id, uuid.UUID)
    try:
      return self.success(self.device(id = str(id),
                                      owner = self.user['_id']))
    except error.Error as e:
      self.fail(*e.args)

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
    query = {'id': str(id), 'owner': owner['_id']}
    if self.device(ensure_existence = False, **query) is not None:
      self.fail(error.DEVICE_ALREADY_REGISTRED)
    to_save = copy.deepcopy(query)
    to_save['name'] = name.strip()
    self.database.devices.insert(to_save, upsert = True)
    import papier
    to_save['passport'] = papier.generate_passport(
      str(id),
      name,
      owner['public_key'],
      conf.INFINIT_AUTHORITY_PATH,
      conf.INFINIT_AUTHORITY_PASSWORD
    )
    self.database.devices.update(query, to_save)
    device = self.database.devices.find_one(query)
    assert device is not None
    # XXX check unique device ?
    self.database.users.find_and_modify({'_id': owner['_id']}, {'$addToSet': {'devices': str(id)}})
    return device

  @api('/device/create', method="POST")
  @require_logged_in
  def create_device(self,
                    id = None,
                    name = None):
    if id is not None:
      assert isinstance(id, uuid.UUID)
    else:
      id = uuid.uuid4()

    device = self._create_device(owner = self.user, id = id, name = name)
    assert device is not None
    return self.success({"id": device['id'],
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
      return self.device(id = str(device_id),
                         owner =  user_id,
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
    assert isinstance(id, uuid.UUID)
    user = self.user
    assert user is not None
    query = {'id': str(id), 'owner': user['_id']}
    try:
      device = self.device(**query)
    except error.Error as e:
      self.fail(*e.args)
    if not str(id) in user['devices']:
      self.fail(error.DEVICE_DOESNT_BELONG_TOU_YOU)
      self.database.device.update(query, {"$set": {"name": name}})
    return self.success({
        'id': str(id),
        'passport': device['passport'],
        'name' : name,
      })

  @api('/device/delete', method = "POST")
  @require_logged_in
  def delete_device(self,
                    id: uuid.UUID):
    """Delete a device.
    """
    assert isinstance(id, uuid.UUID)
    user = self.user
    assert user is not None
    query = {'id': str(id), 'owner': user['_id']}
    try:
      device = self.device(**query)
    except error.Error as e:
      return self.fail(*e.args)

    if not str(id) in user.get('devices', []):
      self.fail(error.DEVICE_DOESNT_BELONG_TOU_YOU)
    self.database.devices.remove(query)
    self.database.users.update({'_id': user['_id']}, {'$pull': {'devices': str(id)}})
    return self.success({'id': str(id)})
