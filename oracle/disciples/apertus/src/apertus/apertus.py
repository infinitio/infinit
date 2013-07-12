from __future__ import print_function

from twisted.internet.protocol import Factory, Protocol
from twisted.protocols.basic import LineReceiver
from twisted.internet import reactor
from pprint import pprint

import json

class Apertcpus(Protocol):
    def __init__(self, factory, addr):
        self.factory = factory
        self.addr = addr

    def connectionMade(self):
        print(self.connectionMade)
        cached_l = self.factory.cache
        if self.factory.cache:
            self.factory.cache = []
        for cached in cached_l:
            self.transport.write(cached)

    def dataReceived(self, data):
        if len(self.factory.clients) == 1:
            print("caching data")
            self.factory.cache.append(data)
        else:
            for addr, client in self.factory.clients.items():
                if addr == self.addr:
                    continue
                client.transport.write(data)

class ApertcpusFactory(Factory):
    def __init__(self):
        self.clients = {}
        self.cache = []

    def buildProtocol(self, addr):
        self.clients[addr] = Apertcpus(self, addr)
        return self.clients[addr]

    def stopFactory(self):
        for client in self.clients:
            client.loseConnection()

    def __str__(self):
        return "<ApertcpusFactory(port={port}, clients={clients})>".format(self.__dict__)

    @property
    def port(self):
        return self._port

    @port.setter
    def port(self, value):
        self._port = value

class ApertusMaster(LineReceiver):
    """
    This class is intended to control the Apertus servers.
    """

    delimiter = "\n"

    def __init__(self, addr, factory):
        self.addr = addr
        self.factory = factory

    def connectionMade(self):
        pass

    def handle_add_link(self, data):
        """handle_add_link add a link between two peers"""
        id = data["_id"]
        new_apertus = ApertcpusFactory()
        port = reactor.listenTCP(0, new_apertus)
        new_apertus.port = port
        new_apertus.id = id
        self.factory.slaves.append(new_apertus)
        print("create new link at", port)
        msg = {"endpoint" : "{}:{}".format(self.addr, port.getHost().port), "_id" : id}
        self.sendLine(json.dumps(msg))

    def handle_del_link(self, data):
        """handle_del_link del a link between two peers"""
        id = data["_id"]
        print("slaves are:", self.factory.slaves)
        for slave in self.factory.slaves:
            print("slave is", slave)
            if slave.id == id:
                slave.doStop()
                self.factory.slaves.remove(slave)
        pprint(self.factory.slaves)

    def lineReceived(self, line):
        print(line)
        data = json.loads(line)
        request = data["request"]
        hdl = getattr(self, "handle_{}".format(request), None)
        if hdl is not None:
            hdl(data)
        else:
            print("unhandled command {}".format(request))

class Factory(Factory):
    def __init__(self, addr, clients):
        self.ap_addr = addr
        self.slaves = clients

    def buildProtocol(self, addr):
        return ApertusMaster(self.ap_addr, self)
