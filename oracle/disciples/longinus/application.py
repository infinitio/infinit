from __future__ import print_function

import sys

from twisted.internet import reactor
from twisted.python import log

import longinus
import meta.conf

class Application(object):
    def __init__(self, ip=meta.conf.LONGINUS_HOST, port=meta.conf.LONGINUS_PORT):
        self.ip = ip
        self.port = port
        pass

    def run(self):
        log.startLogging(sys.stderr)
        reactor.listenUDP(self.port, longinus.PunchHelper())
        reactor.run()
