#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import socket

import os.path
import pythia
import subprocess
import shlex
import atexit
import time

root_dir = subprocess.check_output(shlex.split("git rev-parse --show-toplevel"))
root_dir = root_dir.strip().decode('utf-8')
print(root_dir)

class Trophonius:
    def __init__(self, host="0.0.0.0", port="4242", meta_url="http://localhost:8080"):
        self.host = host
        self.port = port
        self.control_port = str(int(port) + 1)
        self.meta_url = meta_url
        self.instance = None

    def __enter__(self):
        self.instance = subprocess.Popen(
                ["python",
                    os.path.join(root_dir, "_build", "linux64", "bin", "trophonius-server"),
                    "--port", self.port,
                    "--control-port", self.control_port,
                    "--meta-url", self.meta_url,
                ]
        )
        return self

    def __exit__(self, exception, exception_type, backtrace):
        assert self.instance is not None
        self.instance.terminate()
        time.sleep(1)

class FakeMeta:
    def __init__(self, host="0.0.0.0", port="8080"):
        self.host = host
        self.port = port
        self.url = "http://{}:{}/".format(self.host, self.port)
        self.instance = None

    def __enter__(self):
        self.instance = subprocess.Popen(
                ["python",
                    os.path.join(root_dir, "tests", "unit", "oracle", "fake_meta.py")
                ]
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
with FakeMeta(port="8080") as fake_meta, Trophonius(port="4242") as tropho:
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
    print(resp)
    if "msg" in resp and resp["msg"] == "coucou":
        res = 0
    else:
        res = 1
exit(res)
