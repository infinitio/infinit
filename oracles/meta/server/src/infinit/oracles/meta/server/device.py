# -*- encoding: utf-8 -*-

import bottle
import bson
import copy
import uuid
import elle.log

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
      elle.log.trace('Could not fetch device %s for owner %s' % (id, owner))
      raise error.Error(error.DEVICE_NOT_FOUND)
    return device

  def remove_devices(self, user):
    self.database.devices.remove({"owner": user['_id']})

  @property
  def current_device(self):
    device = bottle.request.session.get('device')
    if device is not None:
      assert isinstance(device, bson.ObjectId)
      return self.database.devices.find_one({'_id': device})
    return None


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
                     id = None,
                     device_push_token = None):
    """Create a device.
    """
    with elle.log.trace('create device %s with owner %s' %
                        (id, owner['_id'])):
      if id is not None:
        assert isinstance(id, uuid.UUID)
      else:
        id = uuid.uuid4()
      id = str(id)
      if name is None:
        name = id
      if regexp.Validator(regexp.DeviceName,
                          error.DEVICE_NOT_VALID)(name):
        self.fail(error.DEVICE_NOT_VALID)
      import papier
      device = {
        'id': id,
        'name': name.strip(),
        'owner': owner['_id'],
        'passport': papier.generate_passport(
          id,
          name,
          owner['public_key'],
          conf.INFINIT_AUTHORITY_PATH,
          conf.INFINIT_AUTHORITY_PASSWORD
        ),
      }
      if device_push_token is not None:
        device['push_token'] = device_push_token
      try:
        self.database.devices.insert(device)
      except pymongo.errors.DuplicateKeyError:
        self.fail(error.DEVICE_ALREADY_REGISTERED)
      self.database.users.find_and_modify(
        {'_id': owner['_id']},
        {'$addToSet': {'devices': id}},
      )
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
        raise error.Error(error.DEVICE_DOESNT_BELONG_TO_YOU)
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
      self.fail(error.DEVICE_DOESNT_BELONG_TO_YOU)
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
      self.fail(error.DEVICE_DOESNT_BELONG_TO_YOU)
    self.database.devices.remove(query)
    self.database.users.update({'_id': user['_id']}, {'$pull': {'devices': str(id)}})
    return self.success({'id': str(id)})
