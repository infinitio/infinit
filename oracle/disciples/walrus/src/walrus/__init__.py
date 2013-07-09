#!/usr/bin/env python3

import socket
import subprocess
import socket
import argparse
import sys
import os
import tempfile

walrus_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "walrus.py")

class Walrus:
    def __init__(self, port=0, control_port=0, trophonius_host="127.0.0.1", trophonius_port=0):
        self.__port = port
        self.__control_port = control_port
        self.thost = trophonius_host
        self.tport = trophonius_port
        self.__directory = tempfile.mkdtemp()
        self.control = None

    def __enter__(self):
        import time
        args = [walrus_path,
            "--port", str(self.__port),
            "--control-port", str(self.__control_port),
            "--trophonius-host", self.thost,
            "--trophonius-port", str(self.tport),
            "--runtime-dir", self.__directory,
            ]
        print(args)
        self.instance = subprocess.Popen(args)
        time.sleep(2)
        self.control = socket.create_connection(("127.0.0.1", self.control_port()))
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

    def pause(self):
        self.control.send(b"pause\n")

    def __exit__(self, exception_type, exception, traceback):
        self.instance.terminate()
