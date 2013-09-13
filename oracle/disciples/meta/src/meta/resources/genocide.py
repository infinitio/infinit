# -*- encoding: utf-8 -*-

import json
import meta
import web

import metalib

from meta import conf, database
from meta.page import Page
from meta import error
from meta import regexp
import meta.notifier

from pythia.constants import ADMIN_TOKEN

class All(Page):
    """
    Make all client commit suicide.
    """

    __pattern__ = "/genocide"

    def POST(self):

        if self.data['admin_token'] != ADMIN_TOKEN:
            return self.error(error.UNKNOWN, "You're not admin")

        # XXX: add broadcast capability to trophonius.
        users = list(user['_id']
                     for user
                     in self.database.users.find({'connected': True}))
        self.notifier.notify_some(meta.notifier.SUICIDE,
                                  message = {},
                                  recipient_ids = users,
                                  store = False)

        return self.success({'victims': users})
