#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import socket

import errno
import os.path
import pythia
import subprocess
import shlex
import atexit
import tempfile
import time

root_dir = subprocess.check_output(shlex.split("git rev-parse --show-toplevel"))
root_dir = root_dir.strip().decode('utf-8')

class Trophonius:
    def __init__(self,
                 host = "0.0.0.0",
                 port = 0,
                 control_port = 0,
                 meta_url = "http://localhost:8080"):
        self.host = host
        self.port = port
        self.control_port = control_port
        self.meta_url = meta_url
        self.instance = None
        self.__directory = tempfile.TemporaryDirectory()

    def __read_port(self, path):
        with open(os.path.join(self.__directory.name, path), 'r') as f:
            return(int(f.readline()))


    def __enter__(self):
        self.__directory.__enter__()
        self.instance = subprocess.Popen(
            ["python",
             os.path.join(root_dir, "_build", "linux64", "bin", "trophonius-server"),
             "--port", str(self.port),
             "--control-port", str(self.control_port),
             "--meta-url", self.meta_url,
             "--runtime-dir", self.__directory.name,
            ],
            stdout = subprocess.PIPE,
            stderr = subprocess.PIPE,
        )
        while True:
            try:
                self.port = self.__read_port('trophonius.sock')
                self.control_port = self.__read_port('trophonius.csock')
                break
            except OSError as e:
                if e.errno is not errno.ENOENT:
                    raise
            time.sleep(1)
        return self



    def __exit__(self, exception, exception_type, backtrace):
        assert self.instance is not None
        self.instance.terminate()
        while True:
            try:
                self.__read_port('trophonius.sock')
                time.sleep(1)
            except:
                try:
                    self.__read_port('trophonius.csock')
                    time.sleep(1)
                except:
                    break
        self.__directory.__exit__(exception,
                                  exception_type,
                                  backtrace)

class FakeMeta:
    def __init__(self, host = '0.0.0.0', port = 0):
        self.host = host
        self.port = port
        self.url = "http://{}:{}/".format(self.host, self.port)
        self.instance = None

    def __enter__(self):
        self.instance = subprocess.Popen(
            ["python",
             os.path.join(root_dir, "tests", "unit", "oracle", "fake_meta.py")
            ],
            stdout = subprocess.PIPE,
            stderr = subprocess.PIPE,
        )
        return self

    def __exit__(self, exception, exception_type, backtrace):
        assert self.instance is not None
        self.instance.terminate()

class Client:
    def __init__(self, addr, user_id, device_id, token):
        self.connection = socket.create_connection(addr)
        self.socket = self.connection.makefile()
        self.device_id = device_id
        self.user_id = user_id
        self.sendline({'token': token, "device_id": device_id, "user_id": user_id})

    def sendline(self, data):
        message = json.dumps(data)
        self.connection.sendall(bytes(message + "\n", encoding="utf-8"))

    def readline(self):
        return self.socket.readline()

class Admin(Client):
    def __init__(self, addr):
        self.connection = socket.create_connection(addr)
        self.socket = self.connection.makefile()

    def notify(self, to, msg):
        data = {'to': to}
        data.update(msg)
        self.sendline(data)

res = 1
with FakeMeta(port="8080") as fake_meta, Trophonius() as tropho:
    time.sleep(1)
    meta_client = pythia.Client(server=fake_meta.url)

    res = meta_client.post("/user/login", {
        "email": "pif@infinit.io",
        "password": "paf"
        })

    c = Client((tropho.host, tropho.port), token=res["token"], device_id="pif", user_id=res["_id"])
    admin = Admin((tropho.host, tropho.control_port))

    admin.notify(to=c.user_id, msg={"msg": "coucou"})

    # First we received the message confirming the connection
    resp = json.loads(c.readline())
    if resp["response_code"] != 200:
        res = 1

    # Then, we wait for the notification we sent
    resp = json.loads(c.readline())
    if "msg" in resp and resp["msg"] == "coucou":
        res = 0
    else:
        res = 1
exit(res)
