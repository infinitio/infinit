
from __future__ import print_function

import sys
import itertools as it

from twisted.internet.protocol import DatagramProtocol
from twisted.python import log

from pprint import pprint

class PunchHelper(DatagramProtocol):

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

    def handle_echo(self, (host, port), type, *args):
        print("handle_echo {}".format(type))
        msg = "{}:{}".format(host, port)
        if type == "sasp": #Same Address Same Port
            print("send {} to {}:{}".format(msg, host, port))
            self.transport.write("{}\n".format(msg), (host, port))
        elif type == "dadp": #Different Address Different Port
            pass
        elif type == "sadp": #Same Address Different Port
            self.transport.write(msg, (host, port))

    def handle_ping(self, (host, port), *args):
        print("Keep Alive from {}:{}".format(host, port))
        self.transport.write("pong\n", (host, port))

    def datagramReceived(self, data, (host, port)):
        print("received {} from {}:{}".format(data, host, port))
        cmd_line = data.split()
        cmd = cmd_line[0]
        hdl = getattr(self, "handle_{}".format(cmd), None)
        if hdl is not None:
            hdl((host, port), *cmd_line[1:])
