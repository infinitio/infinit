import json
import web

from meta.page import Page
from meta import database
from meta import error
from meta import regexp
from meta import notifier

import meta.mail

import re

import metalib

def TRANSACTION_STATUS(name, value):
    globals()[name.upper()] = value
    _status_to_string[value] = str(name)

filepath = os.path.abspath(
  os.path.join(os.path.dirname(__file__), 'transaction_status.hh.inc')
)

configfile = open(filepath, 'r')
for line in configfile:
    eval(_macro_matcher.sub(replacer, line))

class All(Page):
    """
    POST {
              'count': the number of notif you want to pull.
              'offset': the offset.
         }
        -> {
                'notifs': [{}, {}, {}]
                'old_notifs': [{}, {}]
           }
    """

    __pattern__ = "/notifications"

    def POST(self):
        if not self.user:
            return self.error(error.NOT_LOGGED_IN)

        notifications = self.user['notifications']
        count = self.data.get('count', 10)
        offset = min(self.data.get('offset', 0), len(notifications) - 1)

        notifs = notifications[offset:]
        old_notifs = []
        if len(notifs) < count:
            old_notifs = self.user['old_notifications'][:(count - len(notifs))]
            old_notifs.reverse() # XXX remove

        return self.success({
            'notifs' : notifs,
            'old_notifs': old_notifs,
        })

# XXX backward compat
class Get(All):
    __pattern__ = "/notification/get"

class Read(Page):
    """
    GET -> {
        'success': True
    }
    """

    __pattern__ = "/notification/read"

    def GET(self):
        if not self.user:
            return self.error(error.NOT_LOGGED_IN)

        self._user['notifications'].reverse() # XXX remove
        old = []
        new = []
        for n in self._user['notifications']:
            if n['notification_type'] == notifier.TRANSACTION and \
               n['status'] and in [CANCELLED, FAILED, FINISHED]:
                new.append(n)
            else:
                old.append(n)

        self._user['old_notifications'].insert(0, old)
        self._user['notifications'] = new

        database.users().save(self._user)

        return self.success()
