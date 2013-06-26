import json
import socket

import os.path
import pythia
import argparse
import subprocess
import shlex
import atexit
import time

parser = argparse.ArgumentParser(description="Trophonius test")

parser.add_argument(
    '--port', type = int,
    default = 4242
)

parser.add_argument(
    '--control-port', type = int,
    default = 4343
)

parser.add_argument(
    '--host', type = str,
    default = "localhost"
)

parser.add_argument(
    '--meta-url',
    type = str,
    default = "http://localhost:8080"
)



args = parser.parse_args()

root_dir = subprocess.check_output(shlex.split("git rev-parse --show-toplevel"))
root_dir = root_dir.strip()
print(root_dir)
 
meta = subprocess.Popen(["python",
        os.path.join(root_dir, "tests", "unit", "oracle", "fake_meta.py")])

tropho = subprocess.Popen(["python",
        os.path.join(root_dir, "_build", "linux64", "bin", "trophonius-server"),
        "--port", "4242",
        "--control-port", "4343",
        "--meta-url", "http://localhost:8080"])

def kill_server():
    tropho.terminate()
    time.sleep(1)
    meta.terminate()

atexit.register(kill_server)

class Client:
    def __init__(self, (server, port), user_id, device_id, token):
        self.connection = socket.create_connection((server, port)) 
        self.socket = self.connection.makefile()
        self.device_id = device_id
        self.user_id = user_id
        self.sendline({'token': token, "device_id": device_id, "user_id": user_id})

    def sendline(self, data):
        message = json.dumps(data)
        self.connection.sendall(message + "\n")

    def readline(self):
        return self.socket.readline()

class Admin(Client):
    def __init__(self, (server, port)):
        self.connection = socket.create_connection((server, port))
        self.socket = self.connection.makefile()

    def notify(self, to, msg):
        data = {'to': to}
        data.update(msg)
        self.sendline(data)

time.sleep(1)
meta_client = pythia.Client(server=args.meta_url)
res = meta_client.post("/user/login", {
    "email": "pif@infinit.io",
    "password": "paf"
    })

c = Client((args.host, args.port), token=res["token"], device_id="pif", user_id=res["_id"])
admin = Admin((args.host, args.control_port))
admin.notify(to=c.user_id, msg={"msg": "coucou"})

# First we received the message confirming the connection
resp = json.loads(c.readline())
if resp["response_code"] != 200:
    exit(1)

# Then, we wait for the notification we sent
resp = json.loads(c.readline())
print(resp)
if "msg" in resp and resp["msg"] == "coucou":
    exit(0)
else:
    exit(1)

