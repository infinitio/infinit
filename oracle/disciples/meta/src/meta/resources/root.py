# -*- encoding: utf-8 -*-

import json
import time

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
        user = self.database.users.find_one({"email": email})

        if user is None:
            return self.error(error.UNKNOWN_USER)

        # Invalidate all transactions.
        # XXX: Peers should be notified.
        from meta.resources import transaction
        self.database.transactions.update(
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
        invite_user(user['email'], database = self.database)

        return self.success({'ghost': str(user['_id'])})

class ResetAccount(Page):
    """Reset account using the hash generated from the /lost-password page.
        POST -> {
            'password': password,
        }
    """
    __pattern__ = '/reset-account/(.+)'

    def __user_from_hash(self, hash):
        user = self.database.users.find_one({"reset_password_hash": hash})
        if user is None:
            self.raise_error(
                error.OPERATION_NOT_PERMITTED,
                msg = "Your password has already been reset",
            )
        if user['reset_password_hash_validity'] < time.time():
            self.raise_error(
                error.OPERATION_NOT_PERMITTED,
                msg = "The reset url is not valid anymore",
            )
        return user

    def GET(self, hash):
        usr = self.__user_from_hash(hash)
        return self.success({
            'email': usr['email'],
        })

    def POST(self, hash):
        user = self.__user_from_hash(hash)
        from meta.resources import transaction
        self.database.transactions.update(
            {
                "$or": [
                    {"sender_id": user['_id']},
                    {"recipient_id": user['_id']}
                ]
            },
            {
                "$set": {"status": transaction.CANCELED}
            },
            multi = True
        )

        import metalib
        from meta import conf
        identity, public_key = metalib.generate_identity(
            str(user["_id"]),
            user['email'], self.data['password'],
            conf.INFINIT_AUTHORITY_PATH,
            conf.INFINIT_AUTHORITY_PASSWORD
        )
        user_id = self.registerUser(
            _id = user["_id"],
            register_status = 'ok',
            email = user['email'],
            fullname = user['fullname'],
            password = self.hashPassword(self.data['password']),
            identity = identity,
            public_key = public_key,
            handle = user['handle'],
            lw_handle = user['lw_handle'],
            swaggers = user['swaggers'],
            networks = [],
            devices = [],
            connected_devices = [],
            connected = False,
            notifications = [],
            old_notifications = [],
            accounts = [
                {'type': 'email', 'id': user['email']}
            ],
            remaining_invitations = user['remaining_invitations'],
        )
        return self.success({'user_id': str(user_id)})

class DeclareLostPassword(Page):
    """Generate a reset password url.
    POST { 'email': "The mail of the infortunate user" } -> {}
    """

    __pattern__ = '/lost-password'

    def POST(self):
        email = self.data['email'].lower()
        user = self.database.users.find_one({"email": email})
        if not user:
            return self.error(error_code = error.UNKNOWN_USER)
        import time, hashlib
        user['reset_password_hash'] = hashlib.md5(str(time.time()) + email).hexdigest()
        user['reset_password_hash_validity'] = time.time() + RESET_PASSWORD_VALIDITY
        from meta.mail import send_via_mailchimp
        send_via_mailchimp(
          email,
          LOST_PASSWORD_TEMPLATE_ID,
          '[Infinit] Reset your password',
          reply_to = 'support@infinit.io',
          reset_password_hash = user['reset_password_hash'],
        )
        self.database.users.save(user)
        return self.success()

class GetExistingBacktrace(Page):
    """
    Store the existing crash into database and send a mail if set.
    """
    __pattern__ = "/debug/existing-report"

    def POST(self):
        user_name = self.data.get('user_name', 'Unknown')
        client_os = self.data.get('client_os', 'Unknown')
        more = self.data.get('more', [])
        env = self.data.get('env', [])
        version = self.data.get('version', 'Unknown version')
        email = self.data.get('email', "crash@infinit.io") #specified email if set.
        send = self.data.get('send', False) #Only send if set.
        file = self.data.get('file', "")
        if not isinstance(more, list):
            more = [more]

        self.database.crashes.insert(
             {
                 "client_os": client_os,
                 "version": version,
                 "user_name": user_name,
                 "environment": env,
                 "more": more,
             }
        )

        if send:
            import meta.mail
            meta.mail.send(
                email,
                subject = meta.mail.EXISTING_BACKTRACE_SUBJECT % {
                    "client_os": client_os,
                },
                content = meta.mail.EXISTING_BACKTRACE_CONTENT % {
                    "client_os": client_os,
                    "version": version,
                    "user_name": user_name,
                    "env":  u'\n'.join(env),
                    "more": u'\n'.join(more),
                },
                attached = file
                )

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

        self.database.crashes.insert(
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
