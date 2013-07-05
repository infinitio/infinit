# -*- encoding: utf-8 -*-

import os
import sys
import web

from meta import conf
from meta import resources
from meta import database
from meta.session import Session
from meta.session_store import SessionStore

class Application(object):
    """
    Application class wrap web.application.run method
    """
    def __init__(self,
                 meta_host = 'localhost',
                 meta_port = 8080,
                 mongo_host = None,
                 mongo_port = None,
                 port_file = None,
                 no_apertus = False):
        urls = []
        views = {}

        self.port_file = port_file
        self.host = meta_host
        self.port = meta_port
        self.mongo_host = mongo_host
        self.mongo_port = mongo_port
        for resource in resources.ALL:
            id_ = str(id(resource))
            urls.extend([resource.__pattern__, id_])
            print("%s: %s" % (resource.__name__, resource.__pattern__))
            views[id_] = resource

        self.app = web.application(urls, views)

        conf.META_HOST = meta_host
        conf.META_PORT = meta_port
        conf.MONGO_HOST = mongo_host
        conf.MONGO_PORT = mongo_port

        conf.NO_APERTUS = no_apertus

        self.ip = meta_host
        self.port = meta_port

        session = Session(self.app, SessionStore(database.sessions()))
        for cls in views.itervalues():
            cls.__session__ = session

    def run(self):
        """
        Run the web server
        """
        from wsgiref.simple_server import make_server
        httpd = make_server(self.host, self.port, self.app.wsgifunc())
        self.port = httpd.server_port
        if self.port_file != None:
            with open(self.port_file, 'w') as f:
                f.write('meta_host:' + self.host + '\n')
                f.write('meta_port:' + str(self.port) + '\n')
                f.write('mongo_host:' + self.mongo_host + '\n')
                f.write('mongo_port:' + str(self.mongo_port) + '\n')
        httpd.serve_forever()
