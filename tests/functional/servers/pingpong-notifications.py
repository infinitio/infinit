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

if __name__ == '__main__':
    import utils
    with utils.Servers() as (meta, trophonius, apertus):

        with gap.State("localhost", int(meta.meta_port), \
                       "0.0.0.0", int(trophonius.port), \
                       "0.0.0.0", int(apertus.port)) as sender, \
             gap.State("localhost", int(meta.meta_port), \
                       "0.0.0.0", int(trophonius.port), \
                       "0.0.0.0", int(apertus.port)) as recipient:
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
