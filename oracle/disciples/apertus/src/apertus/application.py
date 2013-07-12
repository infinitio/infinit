from __future__ import print_function

import sys


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

import meta.conf

class Application(object):
    def __init__(self,
                 ip = meta.conf.APERTUS_HOST,
                 port = meta.conf.APERTUS_PORT,
                 runtime_dir = ""):
        self.ip = ip
        self.port = port
        self.runtime_dir = runtime_dir

    def local_ip(self):
        import socket
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("gmail.com", 80))
        ip, port = s.getsockname()
        s.close()
        return ip

    def run(self):
        log.startLogging(sys.stderr)

        self.clients = list()

        addr = self.local_ip()
        factory = apertus.Factory(addr, self.clients)

        listening_port = reactor.listenTCP(self.port, factory)

        if self.runtime_dir:
            port = listening_port.getHost().port
            with open(os.path.join(self.runtime_dir, "apertus.sock"), "w+") as portfile:
                portfile.write("control:{}\n".format(port))

        if HAVE_SETPROCTITLE:
            setproctitle("apertus-server")
        reactor.run()
