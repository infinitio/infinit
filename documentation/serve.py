#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import argparse
import os
import mimetypes

from http.server import HTTPServer, BaseHTTPRequestHandler

parser = argparse.ArgumentParser(description = "Serve documentation via HTTP")

parser.add_argument(
    'root',
    default = '.',
    nargs = '?',
)

parser.add_argument(
    'port',
    nargs = '?',
    default = 8080,
    type = int,
)

class MarkdownHandler:

    template = """<!DOCTYPE html>
<html>
<head>
  <title>%(title)s</title>
  <meta name="description" content="" />
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <!-- <link rel="shortcut icon" href="favicon.ico" type="image/x-icon" /> -->
  <link rel="stylesheet" href="styles.css" />
</head>
<body><div id="body_content">%(body)s</div></body>
</html>"""

    def __init__(self):
        import markdown2
        self.md = markdown2.Markdown()

    def __call__(self, request, path):
        request.send_header('Content-Type', 'text/html')
        request.send_header('Content-Encoding', 'utf-8')
        with open(path, 'r') as f:
            html = self.md.convert(f.read())
        doc = {
            'title': path,
            'body': html,
        }
        data = (self.template % doc).encode('utf8')
        request.send_header('Content-Length', len(data))
        request.end_headers()
        request.wfile.write(data)

class RequestHandler(BaseHTTPRequestHandler):

    __file_handlers = {
        'md': MarkdownHandler()
    }
    def out(self, *args):
        buf = b''
        for arg in args:
            if isinstance(arg, str):
                arg = arg.encode('utf8')
            buf += arg
        self.wfile.write(buf)

    def _list_dir(self, path):
        self.out('<html>')
        self.out('<h1>', path, '</h1>')
        self.out('<li>')
        for f in os.listdir(path):
            self.out('<ul>', f, '</ul>')
        self.out('</li>')
        self.out('</html>')

    def _get_file(self, path):
        _, ext = os.path.splitext(path)
        ext = ext.lstrip('.')
        if ext in self.__file_handlers:
            self.__file_handlers[ext](self, path)
            return
        type_, encoding = mimetypes.guess_type(path)
        self.send_header('Content-Type', type_)
        if encoding:
            self.send_header('Content-Encoding', encoding)
        self.send_header('Content-Length', os.path.getsize(path))
        self.end_headers()
        with open(path, 'rb') as f:
            data = f.read(4096)
            while data:
                self.wfile.write(data)
                data = f.read(4096)

    def do_GET(self):
        path = os.path.normpath(
            os.path.abspath(os.path.join(self.server.root, './' + self.path))
        )

        print("GET", self.path, '({})'.format(path))
        assert path.startswith(self.server.root)


        if not os.path.exists(path):
            self.send_error(404, "Nothing here.")
            self.end_headers()
            self.wfile.write(b'<h1>Not found.</h1>')
        else:
            self.send_response(200, "We good")
            if os.path.isdir(path):
                self.send_header('Content-Type', 'text/html')
                self.end_headers()
                self._list_dir(path)
            else:
                self._get_file(path)
        self.wfile.flush()
        print(">>GET", self.path, '({})'.format(path))

def main(args):
    server_address = ('', args.port)
    httpd = HTTPServer(server_address, RequestHandler)
    httpd.root = os.path.normpath(os.path.abspath(args.root))
    print("Serve on {}:{}".format(httpd.server_name, httpd.server_port))
    httpd.serve_forever()

if __name__ == '__main__':
    try:
        mimetypes.init()
        main(parser.parse_args())
    except KeyboardInterrupt:
        pass

