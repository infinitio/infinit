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

  def __init__(self, database):
    self.__database = database
    self.__apns = apns.APNs(
      use_sandbox = True,
      cert_file = conf.INFINIT_APS_CERT_PATH)

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
        critera = {'owner': {'$in': list(recipient_ids)}}
      else:
        assert isinstance(device_ids, set)
        critera = {
          'id': {'$in': [str(device_id) for device_id in device_ids]}
        }
      devices_trophonius = []
      for device in self.database.devices.find(
          critera,
          fields = ['id', 'owner', 'trophonius', 'push_token'],
      ):
        devices_trophonius.append((
          device['id'],
          device['owner'],
          device['trophonius'],
          device.get('push_token'),
        ))
      elle.log.debug('targets: %s' % devices_trophonius)
      # Fetch trophoniuses
      trophonius = dict(
        (record['_id'], record)
        for record in self.database.trophonius.find(
            {
              '_id':
              {
                '$in': [t[2] for t in devices_trophonius],
              }
            },
            fields = ['hostname', 'port', '_id']
        ))
      elle.log.debug('trophonius to contact: %s' % trophonius)
      notification = {'notification': jsonify(message)}
      # Ensure unique push tokens are used.
      push_tokens = set()
      # Freezing slow.
      for device, owner, tropho, push in devices_trophonius:
        if push is not None:
          push_tokens.add(push)
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
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
      for push in push_tokens:
        try:
          pl = self.ios_notification(notification_type,
                                     device,
                                     owner,
                                     message)
          if pl is not None:
            self.__apns.gateway_server.send_notification(push, pl)
        except Exception as e:
          elle.log.err('unable to push notification to %s: %s' % (device, e))

  def ios_notification(self, notification_type, device, owner, message):
    if notification_type is not PEER_TRANSACTION:
      return None
    sender_id = str(message['sender_id'])
    recipient_id = str(message['recipient_id'])
    status = int(message['status'])
    if (str(owner) == sender_id) and (device == message['sender_device_id']):
      if status not in [transaction_status.REJECTED, transaction_status.FINISHED]:
        return None
    elif (str(owner) == recipient_id):
      if status not in [transaction_status.INITIALIZED]:
        return None
    else:
      return None
    custom = { 'i': {
      'type': PEER_TRANSACTION,
      'status': status,
      'sender': sender_id,
      'sender_device': message['sender_device_id'],
      'sender_name': message['sender_fullname'],
      'recipient': recipient_id,
      'recipient_device': message['recipient_device_id'],
      'recipient_name': message['recipient_fullname'],
      'file_count': message['files_count'],
    }}
    return apns.Payload(alert = '',
                        sound = '',
                        badge = 0,
                        content_available = True,
                        custom = custom)
