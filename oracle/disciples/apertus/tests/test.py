#!/usr/bin/env python3.2
# -*- encoding: UTF8 -*-

import apertus
from apertus import Apertus
import socket
import json

with Apertus() as apertus:
    connection = socket.create_connection(("127.0.0.1", apertus.port))
    connection.send(bytes(json.dumps({"_id": "toto", "request": "add_link"}) + '\n', encoding='UTF8'))
    data = json.loads(str(connection.recv(4096), encoding="UTF8").strip("\n"))
    client1 = socket.create_connection(data['endpoint'].split(':'))
    client2 = socket.create_connection(data['endpoint'].split(':'))
    for message in (bytes(msg, encoding="UTF8") for msg in ("Hello", "World\n\n", "Trompe\033tte")):
        client1.send(message)
        assert client2.recv(4096) == message
        client2.send(message)
        assert client1.recv(4096) == message

with Apertus() as apertus:
    admin1 = socket.create_connection(("127.0.0.1", apertus.port))
    admin1.send(bytes(json.dumps({"_id": "toto", "request": "add_link"}) + '\n', encoding='UTF8'))
    data1 = json.loads(str(admin1.recv(4096), encoding="UTF8").strip("\n"))

    admin2 = socket.create_connection(("127.0.0.1", apertus.port))
    admin2.send(bytes(json.dumps({"_id": "toto", "request": "add_link"}) + '\n', encoding='UTF8'))
    data2 = json.loads(str(admin2.recv(4096), encoding="UTF8").strip("\n"))

    print(data1, data2)
    assert data1["endpoint"] == data2["endpoint"]

    client1 = socket.create_connection(data1["endpoint"].split(":"))
    client2 = socket.create_connection(data2["endpoint"].split(":"))
    for message in (bytes(msg, encoding="UTF8") for msg in ("Hello", "World\n\n", "Trompe\033tte")):
        client1.send(message)
        assert client2.recv(4096) == message
        client2.send(message)
        assert client1.recv(4096) == message
