from __future__ import print_function

import sys
import itertools as it
import json

from twisted.internet.protocol import DatagramProtocol
from twisted.protocols.basic import LineReceiver
from twisted.python import log

from pprint import pprint

def make_id(self, (ip, port)):
    return "{}:{}".format(ip, port)

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
        self.links = {}

    def datagramReceived(self, data, (host, port)):
        id = make_id(host, port)
        print("received {} from {}".format(data, id))
        peer = ""
        if id in self.links.keys():
            peer = self.links[id]
        elif id in self.links.values():
            l_peers = [k for k, v in self.links.item() if v == id]
            if len(l_peers) > 1:
                raise Exception("To much peers")
            peer = l_peers[0]
        host, port = peer.split(":")
        self.transport.write(data, (host, int(port)))



    def add_link(self, endpoints):
        l = [make_id(int(f["ip"]), int(f["port"])) for f in endpoints]
        self.links[l[0]] = l[1]
        pass

class ApertusControl(LineReceiver):
    """
    This class is intended to control the Apertus servers.
    """

    delimiter = "\n"

    def __init__(self):
        pass

    def connectionMade(self):
        pass

    def handle_add_link(self, data):
        """handle_add_link add a link between two peers"""
        endpoints = data["endpoints"]
        print(endpoints)
        self.slave.add_link(endpoints)
        pass

    def lineReceived(self, line):
        print(line)
        data = json.loads(line)
        request = data["request"]
        hdl = getattr(self, "handle_{}".format(request), None)
        if hdl is not None:
            hdl(data)
        else:
            print("unhandled command {}".format(request))

