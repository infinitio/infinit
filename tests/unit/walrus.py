#!/usr/bin/env python2

from __future__ import print_function
from twisted.internet import protocol, reactor
from twisted.protocols import basic
from twisted.internet.endpoints import TCP4ClientEndpoint
from twisted.python import log
import subprocess
import socket
import argparse
import sys
import os
import tempfile

THIS_FILE = os.path.realpath(__file__)

arguments = argparse.ArgumentParser()

arguments.add_argument("--port", type=int, default=0)
arguments.add_argument("--control-port", type=int, default=0)
arguments.add_argument("--trophonius-host", type=str, default="development.infinit.io")
arguments.add_argument("--trophonius-port", type=int, default=23456)
arguments.add_argument("--log-file", default=sys.stdout)
arguments.add_argument("--runtime-dir", default="")

args = arguments.parse_args()

trophonius = TCP4ClientEndpoint(reactor, args.trophonius_host, args.trophonius_port)

class Walrus:
    def __init__(self, port=0, control_port=0, trophonius_host="127.0.0.1", trophonius_port=0):
        self.__port = port
        self.__control_port = control_port
        self.thost = trophonius_host
        self.tport = trophonius_port
        self.__directory = tempfile.mkdtemp()

    def __enter__(self):
        import time
        args = [THIS_FILE,
            "--port", str(self.__port),
            "--control-port", str(self.__control_port),
            "--trophonius-host", self.thost,
            "--trophonius-port", str(self.tport),
            "--runtime-dir", self.__directory,
            ]
        print(args)
        self.instance = subprocess.Popen(args)
        time.sleep(2)
        return self

    def _read_ports(self):
        with open(os.path.join(self.__directory, "portfile"), "r") as pf:
            for line in pf:
                if line.startswith("port"):
                    drop, self.__port = line.strip().split(":")
                elif line.startswith("control_port"):
                    drop, self.__control_port = line.strip().split(":")

    def port(self):
        if self.__port == 0:
            self._read_ports()
        return self.__port

    def control_port(self):
        if self.__control_port == 0:
            self._read_ports()
        return self.__control_port

    def __exit__(self, exception_type, exception, traceback):
        self.instance.terminate()

class Echo(protocol.Protocol):
    def __init__(self, wire):
        self.wire = wire

    def dataReceived(self, data):
        self.wire.dataFromPeer(data)

    def connectionLost(self, reason):
        log.msg(reason.getErrorMessage())
        self.wire.transport.abortConnection()

class RemoteEndpointFactory(protocol.Factory):
    def __init__(self, wire):
        self.wire = wire

    def buildProtocol(self, addr):
        return Echo(self.wire)

class Wire(protocol.Protocol):
    def __init__(self, addr):
        self.lhs = addr
        self.rhs = None
        self.pause = False
        self.connection = trophonius.connect(RemoteEndpointFactory(self))
        self.connection.addCallback(self.trophoniusConnectionMade)
        self.data_cache = []

    def trophoniusConnectionMade(self, proto):
        self.rhs = proto
        for data in self.data_cache:
            print("send", len(data), "from cache");
            self.rhs.transport.write(data)
        self.data_cache = None

    def dataFromPeer(self, data):
        if self.pause is False:
            self.transport.write(data)

    def dataReceived(self, data):
        if self.connection.called is False:
            self.data_cache.append(data)
        elif self.rhs is not None and self.pause is False:
            self.rhs.transport.write(data)

    def stop(self):
        print("pausing", self)
        self.pause = True

    def connectionLost(self, reason):
        log.msg(reason.getErrorMessage())

class WireFactory(protocol.Factory):
    def __init__(self, clist):
        self.clist = clist

    def buildProtocol(self, addr):
        print("connection from", addr)
        wire = Wire(addr)
        self.clist.append(wire)
        return wire

class Control(basic.LineReceiver):
    delimiter = "\n"

    def __init__(self, clist):
        self.clist = clist

    def lineReceived(self, line):
        msg = line.strip()
        if msg == "pause":
            for c in self.clist:
                c.stop()

class ControlFactory(protocol.Factory):
    def __init__(self, clist):
        self.clist = clist

    def buildProtocol(self, addr):
        return Control(self.clist)

if __name__ == "__main__":
    print(THIS_FILE)
    client_lists = []
    log.startLogging(sys.stdout)
    port = reactor.listenTCP(args.port, WireFactory(client_lists))
    control_port = reactor.listenTCP(args.control_port, ControlFactory(client_lists))
    if args.runtime_dir:
        with open(os.path.join(args.runtime_dir, "portfile"), "w+") as portfile:
                portfile.write("port:{}\n".format(port.getHost().port))
                portfile.write("control_port:{}\n".format(control_port.getHost().port))
    print("listenning on", port.getHost(), control_port.getHost())
    reactor.run()
