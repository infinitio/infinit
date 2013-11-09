#!/usr/bin/python3

import socket
import json

import re
import os
import sys
import time

from infinit.oracles.notification import notifications

from .plugins.jsongo import jsonify

for name, value in notifications.items():
  globals()[name.upper()] = value

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
    # Check that we either have a list of recipients or devices
    assert (recipient_ids is not None) or (device_ids is not None)
    assert message is not None

    message['notification_type'] = notification_type
    message['timestamp'] = time.time() #timestamp in s.

    devices_trophonius = dict()
    if recipient_ids is not None:
      assert isinstance(recipient_ids, set)
      critera = {'owner': {'$in': list(recipient_ids)}}
    else:
      assert isinstance(device_ids, set)
      critera = {'id': {'$in': [str(device_id) for device_id in device_ids]}}
    # Only the connected ones.
    critera['trophonius'] = {"$ne": None}

    for device in self.database.devices.find(
      critera,
      fields = ['id', 'trophonius'],
    ):
      devices_trophonius[device['id']] = device['trophonius']

    trophonius = dict((record['_id'], record)
                      for record in self.database.trophonius.find(
      {
        "_id":
        {
          "$in": list(set(devices_trophonius.values()))
        }
      }
    ))

    notification = {'notification': jsonify(message)}
    # Freezing slow.
    for device in devices_trophonius.keys():
      s = socket.socket(socket.AF_INET,
                        socket.SOCK_STREAM)
      tropho = trophonius.get(devices_trophonius[device])
      if tropho is None:
        print("unknown trophonius %s" % (devices_trophonius[device]))
        continue
      notification["device_id"] = str(device)
      try:
        s = socket.create_connection(address = (tropho['ip'], tropho['port']),
                                     timeout = 4)
        s.send(bytes(json.dumps(notification) + '\n', 'utf-8'))
      except Exception as e:
        print("unable to contact %s: %s" % (tropho['_id'], e))
      finally:
        s.close()
