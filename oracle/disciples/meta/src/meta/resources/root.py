# -*- encoding: utf-8 -*-

import json

from meta.page import Page
from meta import error, database
from pythia.constants import ADMIN_TOKEN, RESET_PASSWORD_VALIDITY

LOST_PASSWORD_TEMPLATE_ID = 'lost-password'

class Root(Page):
    """
    Give basic server infos
        GET /
            -> {
                "server": "server name",
                "logged_in": True/False,
            }
    """

    __pattern__ = '/'

    def GET(self):
        return self.success({
            'server': 'Meta 0.1',
            'logged_in': self.user is not None,
        })

class Status(Page):
    """
    Page to check server status.
    """
    __pattern__ = "/status"
    def GET(self):
        return self.success({"status" : "ok"})


class Ghostify(Page):
    """
    Turn the user to a ghost.
    """

    __pattern__ = "/ghostify"

    def POST(self):
        if 'admin_token' not in self.data or self.data['admin_token'] != ADMIN_TOKEN:
            return self.error(error.UNKNOWN, "You're not admin")

        email = self.data['email']
        user = database.users().find_one({"email": email})

        if user is None:
            return self.error(error.UNKNOWN_USER)

        # Invalidate all transactions.
        # XXX: Peers should be notified.
        from meta.resources import transaction
        database.transactions().update(
            {"$or": [{"sender_id": user['_id']}, {"recipient_id": user['_id']}]},
            {"$set": {"status": transaction.CANCELED}}, multi=True)

        # Ghostify user.
        ghost = self.registerUser(
            _id = user['_id'],
            email = user['email'],
            register_status = 'ghost',
            notifications = [],
            networks = [],
            swaggers = user['swaggers'],
            accounts = [{'type':'email', 'id': user['email']}],
            remaining_invitations = user['remaining_invitations'],
        )

        from meta.invitation import invite_user
        invite_user(user['email'])

        return self.success({'ghost': str(user['_id'])})

class GetBacktrace(Page):
    """
    Store the crash into database and send a mail if set.
    """
    __pattern__ = "/debug/report"

    def PUT(self):
        # Compatibility with v1
        return self.POST()

    def POST(self):
        _id = self.user and self.user.get('_id', "anonymous") or "anonymous" # _id if the user is logged in.
        email = self.data.get('email', "crash@infinit.io") #specified email if set.
        send = self.data.get('send', False) #Only send if set.
        module = self.data.get('module', "unknown module")
        signal = self.data.get('signal', "unknown reason")
        backtrace = self.data.get('backtrace', [])
        env = self.data.get('env', [])
        spec = self.data.get('spec', [])
        more = self.data.get('more', [])
        version = self.data.get('version', 'unknown version')
        file = self.data.get('file', "")
        if not isinstance(more, list):
            more = [more]

        backtrace.reverse()

        import meta.database
        meta.database.crashes().insert(
             {
                 "version": version,
                 "user": _id,
                 "module": module,
                 "signal": signal,
                 "backtrace": backtrace,
                 "environment": env,
                 "specifications": spec,
                 "more": more,
             }
        )

        if send:
            import meta.mail
            meta.mail.send(
                email,
                subject = meta.mail.BACKTRACE_SUBJECT % {"user": _id, "module": module, "signal": signal},
                content = meta.mail.BACKTRACE_CONTENT % {
                    "version": version,
                    "user": _id,
                    "bt":   u'\n'.join(backtrace),
                    "env":  u'\n'.join(env),
                    "spec": u'\n'.join(spec),
                    "more": u'\n'.join(more),
                },
                attached = file
                )
