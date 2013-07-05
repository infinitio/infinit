#!/usr/bin/env python3

import meta
import trophonius
import gap
import pythia

import os
import time

sender_ok = False
recipient_ok = False

# That logs us.
def register(client, fullname, email):
    client.register(fullname, email, "password", fullname + "_device", "bitebite")

def sender_callback(state, status, transaction_id):
    if status == state.TransactionStatus.canceled:
        raise Exception("Transaction canceled")
    elif status == state.TransactionStatus.failed:
        raise Exception("Transaction failed")
    elif status == state.TransactionStatus.finished:
        global sender_ok
        assert sender_ok == False
        sender_ok = True;

def recipient_callback(state, status,  transaction_id):
    if status == state.TransactionStatus.canceled:
        raise Exception("Transaction canceled")
    elif status == state.TransactionStatus.failed:
        raise Exception("Transaction failed")
    elif status == state.TransactionStatus.finished:
        global recipient_ok
        assert recipient_ok == False
        recipient_ok = True;
    elif status == state.TransactionStatus.created and \
          not state.transaction_is_accepted(transaction_id):
      time.sleep(2)
      state.accept_transaction(transaction_id)

def transaction_callback(state, is_sender, peer, transaction_id, status, is_new):
    assert is_new
    print("{}{}Transaction ({})".format(is_new and "New " or "", is_sender and "sender " or "recipient", transaction_id), status)
    if is_sender:
        assert state.transaction_sender_id(transaction_id) == state._id
        assert state.transaction_recipient_id(transaction_id) == peer
        sender_callback(state, status, transaction_id)
    else:
        assert state.transaction_recipient_id(transaction_id) == state._id
        assert state.transaction_sender_id(transaction_id) == peer
        recipient_callback(state, status, transaction_id)

def connection(meta, trophonius):
    print("meta(%s), trophonius(%s)" % (int(meta.meta_port), int(trophonius.port)))

    with gap.State("localhost", int(meta.meta_port), "0.0.0.0", int(trophonius.port)) as sender, \
         gap.State("localhost", int(meta.meta_port), "0.0.0.0", int(trophonius.port)) as recipient:

        register(sender, fullname="sender", email="sender@infinit.io")
        register(recipient, fullname="recipient", email="recipient@infinit.io")

        from functools import partial
        sender.transaction_callback(partial(transaction_callback, sender, True, recipient._id))
        recipient.transaction_callback(partial(transaction_callback, recipient, False, sender._id))

        def clean(to_send_path, destination_directory):
          if os.path.exists(to_send_path):
            os.remove(to_send_path)
          if os.path.exists(destination_directory):
              import shutil
              shutil.rmtree(destination_directory)

        try:
            # Create the destination folder.
            current_path = os.path.abspath(os.path.curdir)
            to_send_path = os.path.join(current_path, 'to_send')
            destination_directory = os.path.join(current_path, 'destination')

            clean(to_send_path, destination_directory)
            os.makedirs(destination_directory)
            with open(to_send_path, 'w') as to_send:
                to_send.write("I'm a file and I will be sent threw infinit.")

            recipient.set_output_dir(destination_directory)

            sender.send_files(recipient._id, [to_send_path,])

            while not (sender_ok and recipient_ok):
                print("poll: ", sender_ok, recipient_ok)
                sender.poll()
                recipient.poll()
                time.sleep(1)

                print("=======")

            assert os.path.exists(os.path.join(destination_directory, 'to_send'))
        finally:
            # Delete the destination folder.
            clean(to_send_path, destination_directory)

if __name__ == '__main__':
    # XXX: For the moment, there is an interdependence between meta and tropho.
    # As long as it stands, we need to have a control port.
    trophonius_control_port = 39074

    with meta.Meta(spawn_db = True, no_apertus = True,
                   trophonius_control_port = trophonius_control_port) as meta, \
         trophonius.Trophonius(meta_port = meta.meta_port,
                               control_port = trophonius_control_port) as trophonius:
        connection(meta, trophonius)
