# -*- encoding: utf-8 -*-

import bottle
import bson
import copy
import uuid
import elle.log
import pymongo

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
             include_passport = False,
             **kwargs):
    if isinstance(id, uuid.UUID):
      id = str(id)
    query = {'devices.id': id}
    if owner is not None:
      assert isinstance(owner, bson.ObjectId)
      if self.user is not None and self.user['_id'] == owner and not include_passport:
        # No need to query the DB in that case
        matches = list(filter(lambda x: x['id'] == id, self.user['devices']))
        if ensure_existence and len(matches) == 0:
          raise error.Error(error.DEVICE_NOT_FOUND)
        return (len(matches) != 0) and matches[0] or None
      query['_id'] = owner
    user = self.__user_fetch(query, fields = ['devices'])
    if ensure_existence and user is None:
      elle.log.trace('Could not fetch device %s for owner %s' % (id, owner))
      raise error.Error(error.DEVICE_NOT_FOUND)
    return list(filter(lambda x: x['id'] == id, user['devices']))[0]

  def remove_devices(self, user):
    self.database.users.update({'_id': user['_id']},
                               { '$set': { 'devices': []}})

  @property
  def current_device(self):
    device = bottle.request.session.get('device')
    if device is None:
      return None
    else:
      return list(filter(lambda x: x['id'] == device, self.user['devices']))[0]


  @api('/devices')
  @require_logged_in
  def devices(self):
    """All user's devices. """
    return {'devices': self.user.get('devices', [])}

  @api('/device/<id>/view')
  @require_logged_in
  def device_view(self,
                  id: uuid.UUID):
    """Return one user device.
    """
    assert isinstance(id, uuid.UUID)
    try:
      return self.success(self.device(id = str(id),
                                      owner = self.user['_id'],
                                      include_passport = True))
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
      has_id = (id is not None)
      if has_id:
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
      res = None
      def create():
        if has_id:
           res = self.database.users.update(
             {'_id': owner['_id'], 'devices.id': id},
             {'$set': {'devices.$': device}},
           )
        if not has_id or res['n'] == 0:
          self.database.users.update(
            {'_id': owner['_id']},
            {'$push': {'devices': device}},
          )
      self.device_override_push_token(device_push_token, create)
      return device

  def device_override_push_token(self, token, action):
    while True:
      try:
        return action()
      except pymongo.errors.DuplicateKeyError:
        assert token is not None
        self.database.users.update(
          {'devices.push_token': token},
          {'$unset': {'devices.$.push_token': True}}
        )
        continue

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
      if str(device_id) not in map(lambda x: x['id'], user['devices']):
        raise error.Error(error.DEVICE_DOESNT_BELONG_TO_YOU)
      return self.device(id = str(device_id),
                         owner =  user_id).get('trophonius') is not None
    else:
      return self.database.users.find(
        {
          "_id": user_id,
          "devices.trophonius": {"$ne": None},
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
      device = self.device(id = str(id), owner = user['_id'])
    except error.Error as e:
      self.fail(*e.args)
    user = self.database.users.find_and_modify(
                               {'_id': user['_id'], 'devices.id': str(id)},
                               {"$set": {"devices.$.name": name}})
    passport = list(filter(lambda x: x['id'] == str(id), user['devices']))[0]['passport']
    return self.success({
        'id': str(id),
        'passport': passport,
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

    self.database.users.update({'_id': user['_id']}, {'$pull': {'devices': {'id': str(id)}}})
    return self.success({'id': str(id)})
