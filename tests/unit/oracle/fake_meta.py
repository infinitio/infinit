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
    desc = 'Meta server that authenticates everything.'
    parser = argparse.ArgumentParser(description = desc)
    parser.add_argument('--port-file',
                        type = str,
                        default = None,
                        help = 'file to write the listening port to')
    args = parser.parse_args()
    try:
        from wsgiref.simple_server import make_server
        httpd = make_server('', 0, app.wsgifunc())
        if args.port_file is not None:
            with open(args.port_file, 'w') as f:
                print >> f, httpd.server_port
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if args.port_file is not None:
            os.remove(args.port_file)
