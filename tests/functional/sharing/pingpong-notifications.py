#!/usr/bin/env python3.2

import meta
import trophonius
import gap
import time

success = False

# That log us.
def register(client, fullname, email):
    client.register(fullname, email, "password", fullname + "_device", "bitebite")

def recipient_message_callback(client, sender_id, message):
    if message == "PING":
        client.send_message(sender_id,
                            "PONG")

def sender_message_callback(client, sender_id, message):
    if message == "PONG":
        global success
        success = True

def connection(meta, trophonius):
    print("meta(%s), trophonius(%s)" % (int(meta.meta_port), int(trophonius.port)))

    with gap.State("localhost", int(meta.meta_port), "0.0.0.0", int(trophonius.port)) as sender, \
         gap.State("localhost", int(meta.meta_port), "0.0.0.0", int(trophonius.port)) as recipient:

        register(sender, fullname="sender", email="sender@infinit.io")
        register(recipient, fullname="recipient", email="recipient@infinit.io")

        from functools import partial
        recipient.message_callback(partial(recipient_message_callback, recipient))
        sender.message_callback(partial(sender_message_callback, sender))

        sender.send_message(recipient._id, "PING")
        while not success:
            sender.poll()
            recipient.poll()
            time.sleep(1)

if __name__ == '__main__':
    # XXX: For the moment, there is an interdependence between meta and tropho.
    # As long as it stands, we need to have a control port.
    trophonius_control_port = 39074

    with meta.Meta(spawn_db = True,
                   trophonius_control_port = trophonius_control_port) as meta, \
         trophonius.Trophonius(meta_port = meta.meta_port,
                               control_port = trophonius_control_port) as trophonius:
        connection(meta, trophonius)
