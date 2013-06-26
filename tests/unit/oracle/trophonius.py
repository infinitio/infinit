import json
import socket

import pythia
import argparse

parser = argparse.ArgumentParser(description="Trophonius test")

parser.add_argument(
    '--port', type = int,
)

parser.add_argument(
    '--control-port', type = int,
)

parser.add_argument(
    '--host', type = str,
)

parser.add_argument(
    '--meta-url',
    type = str,
    default = pythia.constants.DEFAULT_SERVER
)

args = parser.parse_args()

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

meta_client = pythia.Client(server=args.meta_url)
res = meta_client.post("/user/login", {
    "email": "pif@infinit.io",
    "password": "paf"
    })
print(res["token"])

c = Client((args.host, args.port), token=res["token"], device_id="pif", user_id=res["_id"])
admin = Admin((args.host, args.control_port))
admin.notify(to=c.user_id, msg={"msg": "coucou"})

while 1:
    print(c.readline())
