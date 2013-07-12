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

    def add_link(self, _id, *args):
        request = {
                "request" : "add_link",
                "_id": _id,
        }
        msg = json.dumps(request, default = str)
        self.conn.send("{}\n".format(msg))
        bresp = self.conn.recv(4096)
        try:
            resp = json.loads(bresp)
            return resp["endpoint"]
        except TypeError:
            print("invalid response")

    def del_link(self, _id, *args):
        request = {
                "request" : "del_link",
                "_id": _id,
        }
        msg = json.dumps(request, default = str)
        self.conn.send("{}\n".format(msg))
