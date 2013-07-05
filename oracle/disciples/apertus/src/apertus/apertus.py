from __future__ import print_function

from twisted.internet.protocol import DatagramProtocol, Factory, Protocol
from twisted.protocols.basic import LineReceiver
from twisted.internet import task, reactor
from twisted.python import log
from pprint import pprint

import itertools as it

import json
import sys


class Endpoint(object):
    def __init__(self, ip, port = 0):
        self.ip = ip
        self.port = port

    def __str__(self):
        return "{}:{}".format(self.ip, self.port)

    def __repr__(self):
        return "{}:{}".format(self.ip, self.port)

    def __eq__(self, other):
        return str(self) == str(other)

    @property
    def addr(self):
        return (self.ip, int(self.port))

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

class Apertus(DatagramProtocol):

    def __str__(self):
        if self.transport != None:
            repr = "<Apertus(host={}, id={})>".format(self.get_endpoint(), self.id)
        else:
            repr = "<Apertus(host=None, id={})>".format(self.id)
        return repr

    def __repr__(self):
        return self.__str__()

    def __init__(self, iface, id):
        self.id = id
        self.links = []
        #self.task = task.LoopingCall(self.test_timeout)
        #self.task.start(5 * 60) # check every five seconds
        reactor.listenUDP(0, self, interface=iface)

    def port(self):
        host = self.transport.getHost()
        return host.port

    def get_endpoint(self):
        host = self.transport.getHost()
        return "{}:{}".format(host.host, host.port)

    def datagramReceived(self, data, addr):
        host, port = addr
        id = Endpoint(host, port)
        #print(self, "->", id)
        if id not in self.links:
            self.links.append(id)

        for peer in (l for l in self.links if l.addr != id.addr):
            #print(self, id.addr, "->", peer.addr)
            self.transport.write(data, peer.addr)

    def die(self):
        print("SHUTDOWN", self)
        self.transport.stopListening()

    def debug():
        pprint(self.links)

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
        endpoints = data["endpoints"]
        id = data["_id"]
        print("add_link", endpoints, id)
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
        endpoints = data["endpoints"]
        id = data["_id"]
        print("del_link", endpoints, id)
        print("slaves are:", self.factory.slaves)
        for slave in self.factory.slaves:
            print("slave is", slave)
            print("slave id is:", slave.id)
            print("id is:", id)
            if slave.id == id:
                print("KILL", slave)
                slave.die()
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
    def __init__(self, addr):
        self.ap_addr = addr
        self.slaves = []

    def buildProtocol(self, addr):
        return ApertusMaster(self.ap_addr, self)
