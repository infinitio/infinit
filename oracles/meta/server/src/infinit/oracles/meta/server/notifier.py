#!/usr/bin/python3

import socket
import json

import bson
import re
import os
import sys
import time

import elle.log
from infinit.oracles.notification import notifications

from .plugins.jsongo import jsonify

for name, value in notifications.items():
  globals()[name.upper()] = value

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Notifier'

class Notifier:
  def __init__(self, database):
    self.__database = database
    pass

  @property
  def database(self):
    return self.__database

  def notify_some(self,
                  notification_type,
                  recipient_ids = None,
                  device_ids = None,
                  message = None):
    """Send notification to clients.

    notification_type -- Notification id to send.
    recipient_ids     -- User to send the notification to.
    device_ids        -- Devices to send the notification to.
    message           -- The payload.
    """
    with elle.log.trace("notification(%s): %s to %s" % (notification_type, message, recipient_ids)):
      # Check that we either have a list of recipients or devices
      assert (recipient_ids is not None) or (device_ids is not None)
      assert message is not None

      message['notification_type'] = notification_type
      message['timestamp'] = time.time() #timestamp in s.

      elle.log.debug("message to be sent: %s" % message)

      devices_trophonius = dict()

      if recipient_ids is not None:
        assert isinstance(recipient_ids, set)
        critera = {'owner': {'$in': list(recipient_ids)}}
      else:
        assert isinstance(device_ids, set)
        critera = {'id': {'$in': [str(device_id) for device_id in device_ids]}}
      # Only the connected ones.
      critera['trophonius'] = {"$ne": None}

      elle.log.debug("targets critera: %s" % critera)

      for device in self.database.devices.find(
        critera,
        fields = ['id', 'owner', 'trophonius'],
      ):
        devices_trophonius[(device['id'], device['owner'])] = device['trophonius']

      elle.log.debug("targets: %s" % devices_trophonius)

      trophonius = dict((record['_id'], record)
                        for record in self.database.trophonius.find(
        {
          "_id":
          {
            "$in": list(set(devices_trophonius.values()))
          }
        },
        fields = ['hostname', 'port', '_id']
      ))

      elle.log.debug("trophonius to contact: %s" % trophonius)

      notification = {'notification': jsonify(message)}
      # Freezing slow.
      for device, owner in devices_trophonius.keys():
        s = socket.socket(socket.AF_INET,
                          socket.SOCK_STREAM)
        trophonius_to_contact = devices_trophonius[(device, owner)]
        tropho = trophonius.get(trophonius_to_contact)
        if tropho is None:
          elle.log.err("unknown trophonius %s" % trophonius_to_contact)
          continue
        notification["device_id"] = str(device)
        notification["user_id"] = str(owner)
        elle.log.debug("notification to be sent: %s" % notification)
        try:
          s = socket.create_connection(
            address = (tropho['hostname'], tropho['port']), timeout = 4)
          json_str = json.dumps(notification, ensure_ascii = False) + '\n'
          s.send(json_str.encode('utf-8'))
        except Exception as e:
          elle.log.err("unable to contact %s: %s" % (tropho['_id'], e))
        finally:
          s.close()
