# -*- encoding: utf-8 -*-

import json

from meta.page import Page
from meta import database

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

# class Debug(Page):
#     """
#     This class is for debug purpose only
#     """
#     __pattern__ = "/debug"

#     def POST(self):
#         msg = self.data
#         if self.notifier is not None:
#             self.notifier.send_notification(msg)
#         else:
#             return self.error(error.UNKNOWN, "Notifier is not ready.")
#         return self.success({})

# XXX : Remove that cheat
class ScratchDB(Page):
    """
    Debug function to scratch db and start back to 0.
    """

    __pattern__ = "/scratchit"

    def GET(self):

        return self.success({})

class GetBacktrace(Page):
    """
    Debug function to scratch db and start back to 0.
    """
    __pattern__ = "/debug/report"

    def PUT(self):
        import meta.mail

        _id = self.user and self.user.get('_id', 'anonymous') or 'anonymous'
        # Key in object and value not empty.
        email = 'email' in self.data and self.data['email'] or "crash@infinit.io"
        send = self.data.get('send', True);
        module = self.data.get('module', "unknown module")
        signal = self.data.get('signal', "unknown reason")
        backtrace = self.data.get('backtrace', [])
        env = self.data.get('env', [])
        spec = self.data.get('spec', [])
        more = self.data.get('more', [])
        if not isinstance(more, list):
            more = [more]

        backtrace.reverse()

        meta.database.crashes().insert(
             {
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
            meta.mail.send(
                email,
                meta.mail.BACKTRACE_SUBJECT % {"user": _id, "module": module, "signal": signal},
                meta.mail.BACKTRACE_CONTENT %
                {
                    "user": _id,
                    "bt":   '\n'.join('{}'.format(l) for l in backtrace),
                    "env":  '\n'.join('{}'.format(l) for l in env),
                    "spec": '\n'.join('{}'.format(l) for l in spec),
                    "more": '\n'.join('{}'.format(l) for l in more),
                })
