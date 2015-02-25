#!/usr/bin/python3

import socket
import json

import bson
import re
import os
import sys
import time
import apns

import elle.log
from infinit.oracles.notification import notifications
from . import conf, transaction_status

from .plugins.jsongo import jsonify

for name, value in notifications.items():
  globals()[name.upper()] = value

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Notifier'

class Notifier:

  def __init__(self, database, production):
    cert = conf.INFINIT_APS_CERT_PATH_PRODUCTION \
           if production else conf.INFINIT_APS_CERT_PATH
    self.__database = database
    self.__apns = apns.APNs(
      use_sandbox = not production,
      cert_file = cert)

  @property
  def database(self):
    return self.__database

  def notify_some(self,
                  notification_type,
                  recipient_ids = None,
                  device_ids = None,
                  message = None):
    '''Send notification to clients.

    notification_type -- Notification id to send.
    recipient_ids     -- User to send the notification to.
    device_ids        -- Devices to send the notification to.
    message           -- The payload.
    '''
    with elle.log.trace('notification(%s): %s to %s' %
                        (notification_type, message, recipient_ids)):
      assert (recipient_ids is not None) or (device_ids is not None)
      assert message is not None
      # Build message
      message['notification_type'] = notification_type
      message['timestamp'] = time.time() #timestamp in s.
      elle.log.debug('message to be sent: %s' % message)
      # Fetch devices
      if recipient_ids is not None:
        assert isinstance(recipient_ids, set)
        critera = {'_id': {'$in': list(recipient_ids)}}
      else:
        assert isinstance(device_ids, set)
        critera = {
          'devices.id': {'$in': [str(device_id) for device_id in device_ids]}
        }
      devices_trophonius = []
      for usr in self.database.users.find(
          critera,
          fields = {
            'avatar': False, 'small_avatar': False, 'identity': False,
            'devices.passport': False
          }
      ):
        if 'devices' not in usr:
          continue
        devices = usr['devices']
        if recipient_ids is None:
          devices = filter(lambda x: x['id'] in device_ids, devices)
        for device in devices:
          tropho = device.get('trophonius')
          push = device.get('push_token')
          if tropho is not None or push is not None:
            devices_trophonius.append((
              device['id'],
              usr['_id'],
              tropho,
              push,
            ))
      elle.log.debug('targets: %s' % devices_trophonius)
      # Fetch trophoniuses
      trophonius = dict(
        (record['_id'], record)
        for record in self.database.trophonius.find(
            {
              '_id':
              {
                '$in': [t[2] for t in devices_trophonius
                        if t[2] is not None],
              }
            },
            fields = ['hostname', 'port', '_id']
        ))
      elle.log.debug('trophonius to contact: %s' % trophonius)
      notification = {'notification': jsonify(message)}
      # Ensure unique push tokens are used.
      used_push_tokens = set()
      # Freezing slow.
      for device, owner, tropho, push in devices_trophonius:
        if push is not None and push not in used_push_tokens:
          try:
            pl = self.ios_notification(notification_type,
                                       device,
                                       owner,
                                       message)
            if pl is not None:
              used_push_tokens.add(push)
              elle.log.debug('pushing notification to: %s' % device)
              self.__apns.gateway_server.send_notification(push, pl)
          except Exception as e:
            elle.log.err('unable to push notification to %s: %s' % (device, e))
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if tropho is None:
          continue
        tropho = trophonius.get(tropho)
        if tropho is None:
          continue
        notification['device_id'] = str(device)
        notification['user_id'] = str(owner)
        elle.log.debug('notification to be sent: %s' % notification)
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

  def ios_notification(self, notification_type, device, owner, message):
    if notification_type is not PEER_TRANSACTION:
      return None
    sender_id = str(message['sender_id'])
    recipient_id = str(message['recipient_id'])
    status = int(message['status'])
    content_available = False
    badge = 0
    sound = None
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
    return apns.Payload(alert = alert,
                        badge = badge,
                        sound = sound,
                        content_available = content_available)
