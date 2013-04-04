from __future__ import print_function

from twisted.internet.protocol import DatagramProtocol, Factory
from twisted.protocols.basic import LineReceiver
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

class Link(object):
    def __init__(self, source, sink):
        self.source = source
        self.sink = sink
        self.active = False

    def __str__(self):
        return "<Link({}, {}) up={}>".format(self.source, self.sink, self.active)

    def __repr__(self):
        return "<Link({}, {}) up={}>".format(self.source, self.sink, self.active)

    def __cmp__(self, other):
        return (self.source, self.sink, self.active) == (other.source, other.sink, self.active)

    def is_opposite(self, other):
        return (self.source, self.sink) == (other.sink, other.source)

class Apertus(DatagramProtocol):

    def __str__(self):
        attrs = []
        if getattr(self, "local_endpoint", None):
            attrs.append("local={}".format(self.local_endpoint))
        if getattr(self, "public_endpoint", None):
            attrs.append("public={}".format(self.public_endpoint))
        if attrs:
            repr = "<" + self.__class__.__name__ + " " + ", ".join(attrs) + ">"
        else:
            repr = "<" + self.__class__.__name__ + ">"
        return repr

    def __repr__(self):
        return self.__str__()

    def __init__(self):
        self.links = []

    def datagramReceived(self, data, (host, port)):
        id = Endpoint(host, port)
        print("->", id)
        try:
            if id.ip not in (l.source.ip for l in self.links):
                raise KeyError("Not found")
            for p in (l for l in self.links if l.source.port == 0 or l.sink.port == 0):
                # if the current is not set and if the sink is not me
                if p.source.port == 0 and p.sink.port != id.port:
                    p.source.port = port
                    p.active = True
                    break
        except Exception as e:
            print(e)
            print(self.links)
            return

        sender_link = next(l for l in self.links if l.source == id)

        sources = frozenset(l for l in self.links if l.source == sender_link.source and l.active)
        sinks = frozenset(l for l in self.links if l.sink == sender_link.source and l.active)
        peers = []
        print(sources)
        print(sinks)
        for source in sources:
            for sink in sinks:
                if source.is_opposite(sink) and \
                   source.active and sink.active:
                    peers.append(source)
        print(peers)
        for peer in peers:
            print(id.addr, "->", peer.sink.addr)
            self.transport.write(data, peer.sink.addr)

    def add_link(self, endpoints):
        """
        endpoints is a list of lists of endpoints

        and we connect each one of the list to all the other from the other
        list
        """
        rhs_, lhs_ = endpoints
        rhs = [Endpoint(f["ip"]) for f in rhs_]
        lhs = [Endpoint(f["ip"]) for f in lhs_]

        for k, v in it.product(rhs, lhs):
            # Make a 1 to 1 link k -> v, v -> k
            self.links.append(Link(k, v))
            self.links.append(Link(v, k))


    def del_link(self, endpoints):
        """
        endpoints is a list of lists of endpoints

        we have to find each relation a<->b into the map
        """
        rhs_, lhs_ = endpoints
        rhs = [Endpoint(f["ip"], int(f["port"])) for f in rhs_]
        lhs = [Endpoint(f["ip"], int(f["port"])) for f in lhs_]
        for k, v in it.product(rhs, lhs):
            # Remove the 1 to 1 link
            try:
                self.links.remove(next(l for l in self.links if l.source.ip == k.ip))
                self.links.remove(next(l for l in self.links if l.source.ip == v.ip))
            except:
                pass

    def get_endpoint(self):
        host = self.transport.getHost()
        return "{}:{}".format(host.host, host.port)

    def debug():
        pprint(self.links)

class ApertusMaster(LineReceiver):
    """
    This class is intended to control the Apertus servers.
    """

    delimiter = "\n"

    def __init__(self, slave):
        self.slave = slave

    def connectionMade(self):
        pass

    def handle_add_link(self, data):
        """handle_add_link add a link between two peers"""
        endpoints = data["endpoints"]
        print("add_link", endpoints)
        self.slave.add_link(endpoints)
        pprint(self.slave.links)

    def handle_del_link(self, data):
        """handle_del_link del a link between two peers"""
        endpoints = data["endpoints"]
        print("del_link", endpoints)
        self.slave.del_link(endpoints)
        pprint(self.slave.links)

    def handle_get_endpoint(self, data):
        s = {"endpoints" : self.slave.get_endpoint()}
        msg = json.dumps(s)
        self.sendLine(msg)

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

    def buildProtocol(self, addr):
        return ApertusMaster(self.slave)
