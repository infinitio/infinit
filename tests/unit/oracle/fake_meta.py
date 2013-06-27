import argparse
import json
import os
import sys
import web

urls = (
    '/(.*)', 'hello'
)
app = web.application(urls, globals())

class hello:
    def GET(self, name):
        if name == "self" or name == "user/login":
            return json.dumps({'success': True, "_id": "id", 'token':
                'token'})
        return json.dumps({'success': True})
    def POST(self, name):
        return self.GET(name)

if __name__ == "__main__":
    try:
        from wsgiref.simple_server import make_server
        httpd = make_server('', 0, app.wsgifunc())
        print httpd.server_port
        sys.stdout.flush()
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
