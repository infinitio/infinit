from __future__ import print_function

import sys

import platform
if "Linux" in platform.uname():
    from twisted.internet import epollreactor
    epollreactor.install()

from OpenSSL import SSL, crypto

from twisted.internet.protocol import Protocol
from twisted.internet import reactor, ssl
from twisted.python import log

try:
    from setproctitle import setproctitle
    HAVE_SETPROCTITLE = True
except:
    HAVE_SETPROCTITLE = False
    pass

import os
import conf
import apertus
from . import conf

class Application(object):
    def __init__(self,
                 ip = conf.HOST,
                 port = conf.PORT,
                 runtime_dir = "",
                 mongo = None,
                 proxy = False):
        self.ip = ip
        self.port = port
        self.runtime_dir = runtime_dir
        self.mongo = mongo
        self.proxy = proxy

    def local_ip(self):
        import socket
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("gmail.com", 80))
        ip, port = s.getsockname()
        s.close()
        return ip

    def remove_from_db(self):
        import pymongo
        database = self.mongo.apertus
        instances = database.instances
        instances.remove(self.id)

    def unset_id(self, id):
        import pymongo
        print("remove", id, "from set")
        database = self.mongo.apertus
        instances = database.instances
        record = instances.find_one({"ids" : id})
        if record is None:
            # We are not in master/slave mode.
            return
        record["ids"].remove(id)
        instances.save(record)

    def run(self):
        log.startLogging(sys.stderr)
        print("Mongo activated")
        import pymongo
        database = self.mongo.apertus
        instances = database.instances

        if self.proxy:
            factory = apertus.Proxy(instances)
        else:
            self.clients = list()
            addr = self.local_ip()
            factory = apertus.Factory(self, addr, self.clients)

        listening_port = reactor.listenTCP(self.port, factory)

        if not self.proxy:
            import netifaces
            ifaces = [netifaces.ifaddresses(iface)
                    for iface in netifaces.interfaces()
                    if iface != "lo"]
            AUTHORIZED_PROTOCOLS = (netifaces.AF_INET, )
            bound_addresses = []
            local_endpoint = listening_port.getHost()
            for i in ifaces:
                for af, addrs in i.items():
                    if af in AUTHORIZED_PROTOCOLS:
                        for endpoint in addrs:
                            bound_addresses.append((endpoint['addr'],
                                                   local_endpoint.port))
            record = {
                    'endpoints': bound_addresses,
                    'ids' : [],
            }
            self.id = instances.insert(record)

        if self.runtime_dir:
            port = listening_port.getHost().port
            with open(os.path.join(self.runtime_dir, "apertus.sock"), "w+") as portfile:
                portfile.write("control:{}\n".format(port))

        if HAVE_SETPROCTITLE:
            setproctitle("apertus-server ({})".format(self.proxy and "proxy" or "slave"))
        reactor.run()
