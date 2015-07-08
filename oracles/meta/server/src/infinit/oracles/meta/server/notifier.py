#!/usr/bin/python3

import socket
import json
import requests

import bson
import bson.json_util
import re
import os
import sys
import time
import apns

import elle.log
from infinit.oracles.notification import notifications
from . import conf, transaction_status

from .plugins.jsongo import jsonify
import random

for name, value in notifications.items():
  globals()[name.upper()] = value

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Notifier'

# FIXME: Use real API key for production
# This value should ideally be acquired with env vars
API_KEY = 'AIzaSyDUsENEk75dKRuhhpMWFzPT-JgwaqWM7c8'
GCM_URL = 'https://android.googleapis.com/gcm/send'

# FIXME: the notifier is not supposed to be the guy that search the
# database for devices trophoniuses and push token. It's supposed to
# be the guy that notifies. Perform the searches in meta and just
# delegate the notification routing to the notifier. Simplify tests/push-notifications accordingly.
class Notifier:

  fields = {
    'devices.id': True,
    'devices.trophonius': True,
    'devices.push_token': True,
    'devices.os' : True,
    'devices.version' : True,
  }

  def __init__(self, database, production):
    self.__apns = None
    self.__database = database
    self.__production = production

  @property
  def database(self):
    return self.__database

  def build_message(message, notification_type):
    """Add the notification_type and the timestamp in the notification

    message -- The notification to be sent (as a dictionnary).
    notification_type -- The notification type.
    """
    assert message is not None
    # Build message
    message['notification_type'] = notification_type
    message['timestamp'] = time.time() #timestamp in s.
    return message

  def filter_by_version(devices, version, equal = False):
    with elle.log.trace('filter devices %s per version' % devices):
      if version is None:
        return devices
      assert isinstance(version, tuple)
      assert len(version) == 3
      _devices = []
      for device in devices:
        device_version = device.get('version')
        if device_version is None:
          continue
        device_version = (int(device_version['major']),
                          int(device_version['minor']),
                          int(device_version['subminor']))
        if device_version == version if equal else device_version >= version:
          _devices.append(device)
      return _devices

  def build_target(self, user, device):
    """Create a target from a user and a device.

    user -- The recipient of the notification.
    device -- the recipient device of the notification.
    """
    tropho = device.get('trophonius')
    push = device.get('push_token')
    if tropho is not None or push is not None:
      return (
        device['id'],
        user['_id'],
        tropho,
        push,
        device.get('os'),
      )

  def notify_some(self,
                  notification_type,
                  recipient_ids = None,
                  device_ids = None,
                  message = None,
                  version = None,
                ):
    '''Send notification to clients.

    notification_type -- Notification id to send.
    recipient_ids     -- User to send the notification to.
    device_ids        -- Devices to send the notification to.
    message           -- The payload.
    '''
    with elle.log.trace('notify %s (%s) to %s' %
                        (notification_type, message, recipient_ids)):
      assert (recipient_ids is not None) or (device_ids is not None)
      # Fetch devices
      if recipient_ids is not None:
        assert isinstance(recipient_ids, set)
        criteria = {
          '_id':
          {
            '$in': list(recipient_ids),
          }
        }
      else:
        assert isinstance(device_ids, set)
        criteria = {
          'devices.id':
          {
            '$in': [str(device_id) for device_id in device_ids],
          }
        }
      targets = []
      # Find all involved devices
      for user in self.database.users.find(
          criteria,
          fields = Notifier.fields
      ):
        if 'devices' not in user:
          continue
        devices = user['devices']
        if version:
          devices = Notifier.filter_by_version(devices, version)
        if device_ids is not None:
          devices = [d for d in devices if d['id'] in device_ids]
        for device in devices:
          target = self.build_target(user, device)
          if target is not None:
            targets.append(target)
      self.notify_targets(targets, message, notification_type)

  def notify_targets(self, targets, message, notification_type):
    """Send a notification to the given targets.

    targets -- A list of tuple containing targets data.
    message -- The notification to be sent (as a dictionnary).
    notification_type -- The notification type.
    """
    message = Notifier.build_message(message, notification_type)
    elle.log.debug('targets: %s' % targets)
    # Fetch involved trophoniuses
    trophonius = {
      record['_id']: record
      for record in
      self.database.trophonius.find(
        {
          '_id':
          {
            '$in': [t[2] for t in targets
                    if t[2] is not None],
          }
        },
        fields = ['hostname', 'port', '_id']
      )
    }
    elle.log.dump('involved trophoniuses: %s' % trophonius)
    # Freezing slow.
    notification = {'notification': jsonify(message)}
    elle.log.dump('notification: %s' % notification)
    for device, owner, tropho, push, os in targets:
      with elle.log.debug(
          'send notifications to %s (tropho: %s, push: %s)' %
          (device, tropho, push)):
        if push is not None:
          try:
            pl = self.prepare_notification(notification_type,
                                           device,
                                           owner,
                                           message,
                                           os)
            if pl is not None:
              with elle.log.debug(
                  'push notification to: %s' % push):
                self.push_notification(owner, push, pl, os)
            else:
              elle.log.debug('skip push notification for %s' % push)

          except Exception as e:
            elle.log.err('unable to push notification to %s: %s'
                         % (push, e))
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if tropho is None:
          continue
        tropho = trophonius.get(tropho)
        if tropho is None:
          continue
        with elle.log.debug('send notification to: %s' % device):
          notification['device_id'] = str(device)
          notification['user_id'] = str(owner)
          try:
            s = socket.create_connection(
              address = (tropho['hostname'], tropho['port']),
              timeout = 4,
            )
            json_str = \
              json.dumps(notification, ensure_ascii = False) + '\n'
            s.send(json_str.encode('utf-8'))
          except Exception as e:
            elle.log.err('unable to contact %s: %s' %
                         (tropho['_id'], e))
          finally:
            s.close()

  def push_notification(self, recipient_id, token, payload, os):
    if os == 'iOS':
      cert = conf.INFINIT_APS_CERT_PATH_PRODUCTION \
             if self.__production else conf.INFINIT_APS_CERT_PATH
      if self.__apns is None:
        self.__apns = apns.APNs(
          use_sandbox = not self.__production,
          cert_file = cert,
          enhanced=True)
      def apns_error_listener(error_response):
        elle.log.warn('APNS error: %s' % str(error_response))
        self.__apns.gateway_server.force_close()
        self.__apns = apns.APNs(
          use_sandbox = not self.__production,
          cert_file = cert,
          enhanced=True)
        self.__apns.gateway_server.register_response_listener(apns_error_listener)
      self.__apns.gateway_server.register_response_listener(apns_error_listener)
      identifier = random.getrandbits(32)
      self.__apns.gateway_server.send_notification(token, payload, identifier = identifier)
    elif os == 'Android':
      headers = {
          'Authorization' : 'key=%s' % API_KEY,
          'Content-Type' : 'application/json',
          }
      data = {
          'registration_ids' : [token],
          'data' : payload,
          }
      r = requests.post(GCM_URL, headers=headers, data=bson.json_util.dumps(data))
      elle.log.trace('Android notification: %s' % r.content)

  def prepare_notification(self, notification_type, device, owner, message, os):
    content_available = False
    badge = 0
    sound = None
    if notification_type == PEER_TRANSACTION:
      sender_id = str(message['sender_id'])
      recipient_id = str(message['recipient_id'])
      status = int(message['status'])
      if (str(owner) == sender_id) and (device == message['sender_device_id']):
        if status not in [transaction_status.REJECTED, transaction_status.FINISHED]:
          return None
      elif str(owner) == recipient_id:
        if status not in [transaction_status.INITIALIZED]:
          return None
        badge = 1
        sound = 'default'
      else:
        return None
      if sender_id == recipient_id:
        to_self = True
      else:
        to_self = False
      alert = None
      if message['files_count'] == 1:
        files = 'file'
      else:
        files = 'files'
      if status is transaction_status.INITIALIZED: # Only sent to recipient
        if to_self:
          if message['recipient_device_id'] == device:
            alert = "Open Infinit for the transfer to begin"
          else:
            alert = "Accept transfer from another device"
        else:
          alert = "Accept transfer from %s" % message['sender_fullname']
      elif status is transaction_status.REJECTED: # Only sent to sender
        if not to_self:
          alert = 'Canceled by %s' % message['recipient_fullname']
      elif status is transaction_status.FINISHED:
        if to_self:
          alert = 'Transfer received'
        else:
          alert = 'Transfer received by %s' % message['recipient_fullname']
      if alert is None:
        return None
      if os == 'iOS':
        return apns.Payload(alert = alert,
                            badge = badge,
                            sound = sound,
                            content_available = content_available)
      elif os == 'Android':
          return { 'title' : 'Infinit', 'message' : alert }
      else:
        return None
    elif notification_type == NEW_SWAGGER:
      if 'contact_email' not in message:
        return None
      alert = 'Your contact %s (%s) joined Infinit' % \
              (message['contact_fullname'], message['contact_email'])
      if os == 'iOS':
        return apns.Payload(alert = alert,
                            badge = badge,
                            sound = sound,
                            content_available = content_available)
      else:
        return {'title': 'Infinit', 'message': alert}
    else:
      return None
