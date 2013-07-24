# -*- encoding: utf-8 -*-

import json

from meta.page import Page

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

class Status(Page):
    """
    Page to check server status.
    """
    __pattern__ = "/status"
    def GET(self):
        return self.success({"status" : "ok"})


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
