# -*- encoding: utf-8 -*-

import bottle
import bson
import copy
import uuid
import elle.log
import pymongo

from . import conf, error, regexp, notifier
from .utils import *

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
    if hasattr(bottle.request, 'device'):
      return bottle.request.device
    if not hasattr(bottle.request, 'session'):
      return None
    device = bottle.request.session.get('device')
    if device is None:
      return None
    else:
      if self.user is None:
        return None
      user = self.user_by_id(self.user['_id'], fields = ['devices'])
      devices = list(filter(lambda x: x['id'] == device, user['devices']))
      if len(devices):
        device = devices[0]
        bottle.request.device = device
        return device
      return None

  @api('/user/current_device')
  @require_logged_in
  def current_device_view(self):
    return self.device_view(self.current_device)

  def device_view(self, device):
    res = {
      'id': device['id'],
      'name': device['name'],
      'passport': device['passport'],
    }
    if 'os' in device and device['os'] != None:
      res['os'] = device['os']
    if 'last_sync' in device:
      res['last_sync'] = device['last_sync']['date'].isoformat()
    if 'version' in device:
      res['version'] = device['version']
    if 'model' in device:
      res['model'] = device['model']
    return res

  @api('/user/devices')
  @require_logged_in_fields(['devices'])
  def devices_user_api(self):
    return self.devices_users_api(self.user['_id'])

  @api('/devices/<id>', method = "PUT")
  @require_logged_in
  def update_device(self,
                    id: uuid.UUID,
                    name = None,
                    model = None,
                    os = None):
    if name is not None and \
      regexp.Validator(regexp.DeviceName, error.DEVICE_NOT_VALID)(name):
        self.bad_request({
          "reason": "invalid name %s" % name,
          "description": "name must contain 1 to 64 characters"
        })
    user = self.user
    set_dict = {}
    notify_dict = {}
    if name:
      set_dict['devices.$.name'] = name
      notify_dict['name'] = name
    if model:
      set_dict['devices.$.model'] = model
      notify_dict['model'] = model
    if os:
      set_dict['devices.$.os'] = os
      notify_dict['os'] = os
    if set_dict:
      res = self.database.users.find_and_modify(
        {'_id': user['_id'], 'devices.id': str(id)},
        {'$set': set_dict},
        fields = ['devices'],
      )
    else:
      res = self.database.users.find_one(
        {'_id': user['_id'], 'devices.id': str(id)},
        fields = ['devices']
      )
    if res is None:
      self.not_found(
        {
          "reason": "unknown device %s" % id
        })
    device = list(filter(lambda x: x['id'] == str(id), res['devices']))[0]
    notify_dict['id'] = device['id']
    self.notifier.notify_some(
      notifier.MODEL_UPDATE,
      message = {
        'devices': [notify_dict]
      },
      recipient_ids = {user['_id']},
      version = (0, 9, 35))
    return self.device_view(device)

  fields = {
    'devices.id': True,
    'devices.trophonius': True,
    'devices.push_token': True,
    'devices.os' : True,
  }

  delete_device_fields = copy.copy(notifier.Notifier.fields)
  delete_device_fields.update({
    'devices.name': True,
    'devices.online': True,
    'devices.passport': True,
  })

  @api('/devices/<id>', method = 'DELETE')
  @require_logged_in
  def invalidate_device(self, id):
    user = self.user
    fields = notifier.Notifier.fields
    res = self.database.users.find_and_modify(
      {
        '_id': user['_id'],
        'devices.id': id,
      },
      {
        '$pull': {'devices': {'id': id}}
      },
      fields = Mixin.delete_device_fields,
      new = False,
    )
    if res is not None:
      devices = [device for device in res['devices'] if device['id'] == id]
      if len(devices) == 1:
        self.cancel_transactions(user, id)
        self.notifier.notify_some(
          notifier.MODEL_UPDATE,
          message = {
            'devices': [{
              'id': devices[0]['id'],
              '$remove': True,
            }]
          },
          recipient_ids = {user['_id']},
          version = (0, 9, 35))
        # Kickout device.
        self.remove_session(user, device = {'id': id})
        target = self.notifier.build_target(user, devices[0])
        if target is not None:
          self.notifier.notify_targets(
            [target],
            message = {'response_details': 'this device has been deleted'},
            notification_type = notifier.INVALID_CREDENTIALS)
        return {}
    return self.not_found()

  @api('/users/<user>/devices')
  @require_logged_in_or_admin
  def devices_users_api(self, user):
    fields = ['devices']
    if isinstance(user, bson.ObjectId):
      user = self.user_by_id(user, fields = fields)
    else:
      user = self.user_by_id_or_email(user, fields = fields)
    if not self.admin and user['_id'] != self.user['_id']:
      self.forbidden({
        'reason': 'not your devices',
      })
    return self.devices(user)

  def devices(self, user):
    return {
      'devices': [self.device_view(d) for d in user.get('devices', [])],
    }

  @api('/devices/<id>')
  @require_logged_in_or_admin
  def device_api(self, id: uuid.UUID):
    # FIXME: when the old API gets dropped, remove the whole
    # error.Error concept.
    try:
      if self.admin:
        guard = {}
      else:
        guard = {
          'owner': self.user['_id'],
        }
      # FIXME: this 404's even if the device exists but is not
      # ours. Replace with a 403.
      device = self.device(id = id,
                           include_passport = True,
                           **guard)
      return self.device_view(device)
    except error.Error:
      self.not_found({
        'reason': 'device %s does not exist' % id,
        'device': id,
      })

  def _create_device(self,
                     owner,
                     name = None,
                     id = None,
                     OS = None,
                     device_push_token = None,
                     country_code = None,
                     device_model = None):
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
      if OS is not None:
        device['os'] = OS
      if device_push_token is not None:
        device['push_token'] = device_push_token
      if country_code is not None:
        device['country_code'] = country_code
      if device_model is not None:
          device['model'] = device_model
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
      self.notifier.notify_some(
        notifier.MODEL_UPDATE,
        message = {
          'devices': [self.device_view(device)]
        },
        recipient_ids = {owner['_id']},
        version = (0, 9, 35))
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

  def _is_connected(self, user_id, device_id = None):
    """Get the connection status of a given user.

    user_id -- the id of the user.
    """
    assert isinstance(user_id,
                      bson.ObjectId)
    if device_id is not None:
      assert isinstance(device_id, uuid.UUID)
      user = self.user_by_id(user_id)
      if str(device_id) not in map(lambda x: x['id'], user['devices']):
        raise error.Error(error.DEVICE_DOESNT_BELONG_TO_YOU)
      return self.device(id = str(device_id),
                         owner =  user_id).get('trophonius') is not None
    else:
      user = self.database.users.find_one(
        {"_id": user_id},
        fields = ['devices.trophonius'],
      )
      if user is None:
        self.not_found('User not found')
      return any(d.get('trophonius', None) is not None
                 for d in user['devices'])

  ## ------------------- ##
  ## Backward pre-0.9.31 ##
  ## ------------------- ##

  # Anything below this is unused and can (should) be dropped.

  @api('/device/<id>/view')
  @require_logged_in
  def device_view_deprecated(self, id: uuid.UUID):
    """Return one user device.
    """
    try:
      return self.success(self.device(id = id,
                                      owner = self.user['_id'],
                                      include_passport = True))
    except error.Error as e:
      self.fail(*e.args)

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
