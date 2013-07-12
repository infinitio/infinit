#!/usr/bin/env python3.2
# -*- encoding: UTF8 -*-

import apertus
import socket
import json

with apertus.Apertus() as apertus:
    print(apertus.port)
    connection = socket.create_connection(("127.0.0.1", apertus.port))
    connection.send(bytes(json.dumps({"_id": "toto", "request": "add_link"}) + '\n', encoding='UTF8'))
    data = json.loads(str(connection.recv(4096), encoding="UTF8").strip("\n"))
    client1 = socket.create_connection(data['endpoint'].split(':'))
    client2 = socket.create_connection(data['endpoint'].split(':'))
    for message in (bytes(msg, encoding="UTF8") for msg in ("Hello", "World\n\n", "Trompe\033tte")):
        client1.send(message)
        assert client2.recv(4096) == message
