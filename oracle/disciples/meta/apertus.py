# -*- encoding: utf-8 -*-

import socket
import json

import meta.page
import database
from meta import conf

from twisted.python import log
import time

from subprocess import call

import shlex

import re
import os
import sys

class Apertus(object):

    def __init__(self):
        self.conn = socket.socket()
        self.conn.connect((conf.APERTUS_HOST, int(conf.APERTUS_CONTROL_PORT)))

    def add_link(self, endpointsA, endpointsB):
        request = {
                "request" : "add_link",
                "endpoints" : [
                        endpointsA,
                        endpointsB
                    ],
        }
        msg = json.dumps(request, default = str)
        self.conn.send("{}\n".format(msg))

    def del_link(self, *endpoints):
        request = {
                "request" : "del_link",
                "endpoints" : endpoints,
        }
        msg = json.dumps(request, default = str)
        self.conn.send("{}\n".format(msg))

    def get_endpoint(self):
        request = {
                "request" : "get_endpoint",
        }
        msg = json.dumps(request, default = str)
        self.conn.send("{}\n".format(msg))
        bresp = self.conn.recv(4096)
        try:
            resp = json.loads(bresp)
            return  resp["endpoints"]
        except TypeError:
            print("invalid response")
