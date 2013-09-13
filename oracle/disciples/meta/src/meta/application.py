# -*- encoding: utf-8 -*-

import os
import sys
import web
import pymongo

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
                 tropho_control_port = 0,
                 mongo_host = None,
                 mongo_port = None,
                 apertus_host = None,
                 apertus_port = None,
                 port_file = None,
                 fcgi = None):
        urls = []
        views = {}

        self.fcgi = fcgi
        self.port_file = port_file
        self.host = meta_host
        self.port = meta_port
        self.mongo_host = mongo_host
        self.mongo_port = mongo_port
        self.fallback = [(apertus_host, apertus_port)]
        self.tropho_control_port = tropho_control_port
        for resource in resources.ALL:
            id_ = str(id(resource))
            urls.extend([resource.__pattern__, id_])
            print("%s: %s" % (resource.__name__, resource.__pattern__))
            views[id_] = resource
            resource.mongo_host = mongo_host
            resource.mongo_port = mongo_port

        self.app = web.application(urls, views)

        conf.META_HOST = meta_host
        conf.META_PORT = meta_port

        conf.MONGO_HOST = mongo_host
        conf.MONGO_PORT = mongo_port

        self.ip = meta_host
        self.port = meta_port

        # XXX Should be done in Page.
        session = Session(
            self.app,
            SessionStore(
                pymongo.Connection(mongo_host, mongo_port).meta['sessions']
            )
        )
        for cls in views.itervalues():
            cls.__session__ = session
            cls.__application__ = self

    def reset_user_status(self):
        """XXX We reset all user's status to be "disconnected". We know it's
        true because there is only on trophonius instance at the moment.
        The right way to do it is to disconnect all users (in the database)
        when the trophonius instance goes down.
        """
        pymongo.Connection(self.mongo_host, self.mongo_port).meta['users'].update(
            spec = {},
            document = {"$set": {"connected":False, "connected_devices": []}},
            multi = True,
        )

    def run(self):
        """
        Run the web server
        """
        self.reset_user_status()

        print(self.fcgi)
        if self.fcgi:
            print("start fcgi server")
            web.wsgi.runwsgi = lambda func, addr=None: web.wsgi.runfcgi(func, addr)
            self.app.run()
        else:
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
