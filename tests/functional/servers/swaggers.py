#!/usr/bin/env python3.2

import meta
import trophonius
import gap
import time
import pythia

count = 0

# That logs us.
def register(client, fullname, email):
    client.register(fullname, email, "password", fullname + "_device", "bitebite")

def new_swagger_callback(expected, user_id):
    assert(expected == user_id)
    global count
    count += 1

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
            sender.new_swagger_callback(partial(new_swagger_callback, recipient._id))
            recipient.new_swagger_callback(partial(new_swagger_callback, sender._id))

            meta_client = pythia.Admin(server="http://%s:%s" % (meta.meta_host, meta.meta_port))
            res = meta_client.post('/user/add_swagger', {'user1': sender._id, 'user2': recipient._id})

            while count != 2:
                sender.poll()
                recipient.poll()
                time.sleep(1)

            assert len(sender.get_swaggers()) == 1 and recipient._id == sender.get_swaggers()[0]
            assert len(recipient.get_swaggers()) == 1 and sender._id == recipient.get_swaggers()[0]
