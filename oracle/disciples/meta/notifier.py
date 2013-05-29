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
    def notify_one(self, notif_type, to, message, store):
        raise Exception('Not implemented')
    def notify_some(self, notif_type, to, message, store):
        raise Exception('Not implemented')
    def send_notification(self, message):
        raise Exception('Not implemented')
    def close(self):
        raise Exception('Not implemented')

class TrophoniusNotify(Notifier):
    def __init__(self):
        self.conn = socket.socket()

    def open(self):
        self.conn.connect((conf.TROPHONIUS_HOST, int(conf.TROPHONIUS_CONTROL_PORT)))

    def send_notification(self, message):
        if isinstance(message, dict):
            msg = json.dumps(message, default = str)
        # elif isinstance(message, str):
        #     msg = message + json.dumps(_dict, default = str)
        else:
            log.err('Notification was bad formed.')
        # XXX: Log to remove.
        print("{}\n".format(msg))
        self.conn.send("{}\n".format(msg))

    def fill(self, message, notification_type, recipient):
        message['notification_type'] = notification_type
        message['timestamp'] = time.time() #timestamp in s.
        message['to'] = recipient

    def notify_one(self, notification_type, recipient_id, message, store = True):
#      assert isinstance(recipient, database.ObjectId)
        self.fill(message, notification_type, str(recipient_id))

        user_ = self._add_notif_to_db(recipient_id, message)

        if not user_:
            log.err("Unknown user.")
            return

        if user_['connected']:
            self.send_notification(message)

    def notify_some(self, notification_type, recipients_id, message, store = True):
        if not isinstance(recipients_id, list):
            return self.notify_one(notification_type, recipients_id, message)

        # Recipients empty.
        if not recipients_id:
            return

        self.fill(message, notification_type, recipients_id)

        for _id in recipients_id:
            _id = database.ObjectId(_id)
            assert isinstance(_id, database.ObjectId)
            if store:
                user = database.users().find_one(_id)
                user['notifications'].append(message)
                database.users().save(user)

        self.send_notification(message)
