#!/usr/bin/python3

import socket
import json

import re
import os
import sys
import time

_macro_matcher = re.compile(r'(.*\()(\S+)(,.*\))')

def replacer(match):
  field = match.group(2)
  return match.group(1) + "'" + field + "'" + match.group(3)

def NOTIFICATION_TYPE(name, value):
  globals()[name.upper()] = value

filepath = os.path.abspath(os.path.join(os.path.dirname(__file__), 'notification_type.hh.inc'))

configfile = open(filepath, 'r')
for line in configfile:
  eval(_macro_matcher.sub(replacer, line))

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

    devices = dict()
    if recipient_ids is not None:
      assert isinstance(recipient_ids, set)
      critera = {'owner': {'$in': list(recipient_ids)}}
    else:
      assert isinstance(device_ids, set)
      critera = {'_id': {'$in': [str(device_id) for device_id in device_ids]}}
    # Only the connected ones.
    critera['trophonius'] = {"$ne": None}

    print("device to send notification to:")
    for device in self.database.devices.find(
      critera,
      fields = ['_id', 'trophonius'],
    ):
      print(">> %s" % device)
      devices[device['_id']] = device['trophonius'];

    trophonius = dict((record['_id'], record) for  record in self.database.trophonius.find(
      {
        "_id":
        {
          "$in": list(set(devices.values()))
        }
      }
    ))

    # Freezing slow.
    for device in devices.keys():
      s = socket.socket(socket.AF_INET,
                        socket.SOCK_STREAM)
      tropho = trophonius[devices[device]]
      message['device_id'] = str(device)
      print("%s: %s" % (tropho, json.dumps(message)))
      try:
        s = socket.create_connection(address = ('localhost', tropho['port']),
                                     timeout = 4)
        s.send(bytes(json.dumps(message) + '\n', 'utf-8'))
      except Exception as e:
        print("/!\\ unable to contact %s: %s /!\\" % (tropho['_id'], e))
      finally:
        s.close()
