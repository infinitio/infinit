#!/usr/bin/python3

import socket
import json

import re
import os
import sys

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
  def __init__(self, users):
    pass

  def open(self, addr):
    print("connect to trophonius at", addr)
    pass

  def close(self):
    print("disconnect from trophonius")
    pass

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
    pass
