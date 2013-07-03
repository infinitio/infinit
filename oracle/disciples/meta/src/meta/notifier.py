# -*- encoding: utf-8 -*-

import socket
import json

import meta.page
import database
from meta import conf

from twisted.python import log
import time

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

FILE_TRANSFER = 7
FILE_TRANSFER_STATUS = 11
USER_STATUS = 8
MESSAGE = 217
LOGGED_IN = -666
NETWORK_CHANGED = 128

class Notifier(object):
    def open(self):
        raise Exception('Not implemented')
    def close(self):
        raise Exception('Not implemented')

class TrophoniusNotify(Notifier):
    def __init__(self):
        self.conn = socket.socket()

    def open(self):
        self.conn.connect((conf.TROPHONIUS_HOST, int(conf.TROPHONIUS_CONTROL_PORT)))

    def __send_notification(self, message):
        if isinstance(message, dict):
            msg = json.dumps(message, default = str)
        else:
            log.err('Notification was ill formed.')
        self.conn.send(msg + "\n")

    def notify_some(self,
                    notification_type,
                    recipient_ids = None,
                    device_ids = None,
                    message = None,
                    store = True):
        # Check that we either have a list of recipients or devices
        assert (recipient_ids is not None) or (device_ids is not None)
        assert message is not None

        message['notification_type'] = notification_type
        message['timestamp'] = time.time() #timestamp in s.

        if device_ids is None:
            device_ids = set()
        if recipient_ids is None:
            recipient_ids = set()

        device_ids = set(map(database.ObjectId, device_ids))
        recipient_ids = set(map(database.ObjectId, recipient_ids))

        if recipient_ids:
            for recipient_id in recipient_ids:
                user = database.users().find_one(recipient_id)
                device_ids.update(user.get('devices', []))

        message['to_devices'] = list(device_ids)

        if store:
            for device_id in device_ids:
                device = database.devices().find_one(device_id)
                recipient_ids.add(device['owner'])
            for _id in recipient_ids:
                user = database.users().find_one(_id)
                user['notifications'].append(message)
                database.users().save(user)

        self.__send_notification(message)
