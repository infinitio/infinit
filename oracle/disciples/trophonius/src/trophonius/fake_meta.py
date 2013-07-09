#!/usr/bin/env python2.7

import argparse
import json
import os
import sys
import web

urls = (
    '/(.*)', 'hello'
)
app = web.application(urls, globals())
web.config.debug = False

parser = argparse.ArgumentParser(description="Fake Meta server")

parser.add_argument(
    '--port-file',
    default = 'fake_meta_port_file',
)

class hello:
    def GET(self, name):
        if name == "self" or name == "user/login":
            return json.dumps({'success': True, "_id": "id", 'token':
                'token'})
        return json.dumps({'success': True})
    def POST(self, name):
        return self.GET(name)

if __name__ == "__main__":
    args = parser.parse_args()
    if not os.path.exists(os.path.dirname(args.port_file)):
        os.makedirs(os.path.dirname(args.port_file))
    try:
        from wsgiref.simple_server import make_server
        httpd = make_server('', 0, app.wsgifunc())
        if args.port_file != None:
            with open(args.port_file, 'w') as f:
                f.write(str(httpd.server_port))
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
