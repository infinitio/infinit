from __future__ import print_function

from collections import Counter

from twisted.internet.protocol import Factory, Protocol
from twisted.protocols.basic import LineReceiver
from twisted.internet import reactor
from twisted.internet.endpoints import TCP4ClientEndpoint
from pprint import pprint
from twisted.python import log

try:
    from pymongo.objectid import ObjectId
except ImportError:
    from bson.objectid import ObjectId

import json
from functools import partial

class ProxyConnection(LineReceiver):
    delimiter = "\n"
    def __init__(self, parent):
        self.parent = parent

    def lineReceived(self, line):
        print(self.lineReceived, line)
        data = json.loads(line)
        self.parent.parent.sendLine(line)
        self.transport.loseConnection()

    def connectionLost(self, reason):
        print(self.connectionLost, reason.getErrorMessage())
        self.parent.stopFactory()

class ProxyFactory(Factory):
    def __init__(self, parent):
        self.parent = parent

    def buildProtocol(self, addr):
        return ProxyConnection(self)

class Proxyfier(LineReceiver):
    delimiter = "\n"
    def __init__(self, parent):
        self.parent = parent

    def lineReceived(self, line):
        data = json.loads(line)
        record = self.parent.instances.find_one({"ids": data["_id"]})
        print("record is", record)
        if record is not None:
            host, port = record['endpoints'][0]
            client = TCP4ClientEndpoint(reactor, host, port)
        else:
            num = Counter()
            for instance in self.parent.instances.find():
                num[instance["_id"]] -= len(instance['ids'])
            _id, value = num.most_common(1)[0]
            print("found", _id, "with", value, "instances")
            record = self.parent.instances.find_one({"_id": ObjectId(_id)})
            host, port = record["endpoints"][0]
            record["ids"].append(data["_id"])
            self.parent.instances.save(record)
            client = TCP4ClientEndpoint(reactor, host, port)
        connection = client.connect(ProxyFactory(self))
        connection.addCallback(partial(self._connected, data["_id"], connection))

    def _connected(self, id, connection, proto):
        print(self._connected, id, connection)
        data = {
                'request': 'add_link',
                '_id' : id,
        }
        msg = json.dumps(data)
        print("send", msg)
        proto.sendLine(msg)

class Proxy(Factory):
    def __init__(self, instances):
        self.instances = instances

    def buildProtocol(self, addr):
        return Proxyfier(self)

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

    def connectionLost(self, reason):
        print(self.connectionLost, reason.getErrorMessage())
        self.factory.unregister(self)

class ApertcpusFactory(Factory):
    def __init__(self, master):
        self.clients = {}
        self.cache = []
        self.master = master

    def unregister(self, to_remove):
        for addr, client in self.clients.items():
            if client is to_remove:
                del self.clients[addr]
                break
        if len(self.clients) == 0:
            self.doStop()

    def buildProtocol(self, addr):
        self.clients[addr] = Apertcpus(self, addr)
        return self.clients[addr]

    def stopFactory(self):
        for addr, client in self.clients.items():
            client.transport.loseConnection()
        self.master.unregister_slave(self)

    def __str__(self):
        return "<ApertcpusFactory({} clients)>".format(len(self.clients))

    @property
    def port(self):
        return self._port

    @port.setter
    def port(self, value):
        self._port = value

    def __str__(self):
        return "ApertcpusFactory: {}".format(self.port.getHost())

class ApertusMaster(LineReceiver):
    """
    This class is intended to control the Apertus servers.
    """

    delimiter = "\n"

    def __init__(self, addr, factory):
        self.addr = addr
        self.factory = factory

    def __str__(self):
        return self.__repr__()

    def connectionMade(self):
        print(self.connectionMade)

    def connectionLost(self, reason):
        print(self.connectionLost, reason.getErrorMessage())

    def unregister_slave(self, tcp_factory):
        for slave in self.factory.slaves:
            if slave is tcp_factory:
                id = slave.id
                self.factory.slaves.remove(slave)
                self.factory.parent.unset_id(id)

    def handle_add_link(self, data):
        """handle_add_link add a link between two peers"""
        id = data["_id"]
        print(self.handle_add_link, id)
        already_connected_ports = [c.port for c in self.factory.slaves if c.id == id]
        if already_connected_ports:
            # if there is multiple client already connected with the same id,
            # return the port directly. There shall only be one slave for a
            # specific ID.
            assert len(already_connected_ports) == 1
            port = already_connected_ports[0]
            print("recover link at", port.getHost())
        else:
            # else, create a new listenting port, and append it to the slave
            # list.
            new_apertus = ApertcpusFactory(self)
            port = reactor.listenTCP(0, new_apertus)
            new_apertus.port = port
            new_apertus.id = id
            self.factory.slaves.append(new_apertus)
            print("create new link at", port.getHost())
        msg = {"endpoint" : "{}:{}".format(self.addr, port.getHost().port), "_id" : id}
        self.sendLine(json.dumps(msg))

    def handle_del_link(self, data):
        """handle_del_link del a link between two peers"""
        id = data["_id"]
        print(self.handle_del_link, id)
        for slave in self.factory.slaves:
            if slave.id == id:
                slave.doStop()
                self.factory.slaves.remove(slave)
        pprint(self.factory.slaves)

    def lineReceived(self, line):
        data = json.loads(line)
        request = data["request"]
        hdl = getattr(self, "handle_{}".format(request), None)
        if hdl is not None:
            print(hdl)
            hdl(data)
        else:
            print("unhandled command {}".format(request))

class Factory(Factory):
    def __init__(self, parent, addr, clients):
        self.ap_addr = addr
        self.slaves = clients
        self.parent = parent

    def buildProtocol(self, addr):
        return ApertusMaster(self.ap_addr, self)

    def stopFactory(self):
        # remove from db.
        self.parent.remove_from_db()

    def __str__(self):
        return "<Factory({})>".format(self.ap_addr)
