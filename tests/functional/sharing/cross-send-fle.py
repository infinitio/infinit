#!/usr/bin/env python3.2

import gap
import utils

import os
import time

sender_ok = 0
recipient_ok = 0

accepted_transactions=dict()

# That logs us.
def register(client, fullname, email):
    client.register(fullname, email, "password", fullname + "_device", "bitebite")

def sender_callback(state, status, transaction_id):
    global accepted_transactions
    if status == state.TransactionStatus.canceled:
        raise Exception("Transaction canceled")
    elif status == state.TransactionStatus.failed:
        raise Exception("Transaction failed")
    elif status == state.TransactionStatus.finished and \
            not transaction_id in accepted_transactions[state._id]:
        print("finished transaction", transaction_id)
        global sender_ok
        sender_ok += 1

def recipient_callback(state, status, transaction_id):
    global accepted_transactions
    if status == state.TransactionStatus.canceled:
        raise Exception("Transaction canceled")
    elif status == state.TransactionStatus.failed:
        raise Exception("Transaction failed")
    elif status == state.TransactionStatus.finished:
        global recipient_ok
        recipient_ok += 1;
    elif status == state.TransactionStatus.created and \
            not transaction_id in accepted_transactions[state._id]:
        accepted_transactions[state._id].append(transaction_id)
        time.sleep(5)
        state.accept_transaction(transaction_id)

def user_callback(state, is_sender, peer, transaction_id, status, is_new):
    if state._id == state.transaction_sender_id(transaction_id):
        sender_callback(state, status, transaction_id)
    elif state._id == state.transaction_recipient_id(transaction_id):
        recipient_callback(state, status, transaction_id)
    else:
        raise Exception("This transaction doesn't concerne you")

def transaction_callback(state, is_sender, peer, transaction_id, status, is_new):
    assert is_new
    print("{}{}Transaction ({})".format(is_new and "New " or "", is_sender and "sender " or "recipient", transaction_id), status)
    if is_sender:
        sender_callback(state, status, transaction_id)
    else:
        recipient_callback(state, status, transaction_id)

if __name__ == '__main__':
    with utils.Servers(with_apertus = True) as (meta, trophonius):

        print("meta(%s), trophonius(%s)" % (int(meta.meta_port), int(trophonius.port)))

        with gap.State("localhost", int(meta.meta_port), "0.0.0.0", int(trophonius.port)) as sender, \
             gap.State("localhost", int(meta.meta_port), "0.0.0.0", int(trophonius.port)) as recipient:

            register(sender, fullname="sender", email="sender@infinit.io")
            register(recipient, fullname="recipient", email="recipient@infinit.io")

            global accept_transaction
            accepted_transactions[sender._id] = list()
            accepted_transactions[recipient._id] = list()

            from functools import partial
            sender.transaction_callback(partial(user_callback, sender, True, recipient._id))
            recipient.transaction_callback(partial(user_callback, recipient, False, sender._id))

            def clean(to_send_path, destination_directory):
              # if os.path.exists(to_send_path):
              #   os.remove(to_send_path)
              if os.path.exists(destination_directory):
                  import shutil
                  shutil.rmtree(destination_directory)

            try:
                # Create the destination folder.
                current_path = os.path.abspath(os.path.curdir)
                to_send_path = os.path.join(current_path, 'to_send')
                sender_destination_directory = os.path.join(current_path, 'sender_destination')
                recipient_destination_directory = os.path.join(current_path, 'recipient_destination')

                clean(to_send_path, sender_destination_directory)
                os.makedirs(sender_destination_directory)
                clean(to_send_path, recipient_destination_directory)
                os.makedirs(recipient_destination_directory)

                print("creating %s" % to_send_path)
                # with open(to_send_path, 'w') as to_send:
                #     to_send.write("I'm a file and I will be sent threw infinit.")
                #     to_send.write(str(bytearray(1024 * 1024 * 1)))
                print("created %s" % to_send_path)

                recipient.set_output_dir(recipient_destination_directory)
                sender.set_output_dir(sender_destination_directory)

                sender.send_files(recipient._id, [to_send_path,])
                recipient.send_files(sender._id, [to_send_path,])

                while not (sender_ok == 2 and recipient_ok == 2):
                    ## #Progress
                    if len(accepted_transactions) > 0:
                        if len(accepted_transactions[sender._id]):
                            print("sender", sender.transaction_progress(accepted_transactions[sender._id][0]))
                        if len(accepted_transactions[recipient._id]):
                            print("recipient", recipient.transaction_progress(accepted_transactions[recipient._id][0]))
                    print("poll: ", sender_ok, recipient_ok)
                    sender.poll()
                    recipient.poll()
                    time.sleep(1)

                assert os.path.exists(os.path.join(recipient_destination_directory, 'to_send'))
                assert os.path.exists(os.path.join(sender_destination_directory, 'to_send'))
            finally: pass
                # # Delete the destination folder.
                # clean(to_send_path, sender_destination_directory)
                # clean(to_send_path, recipient_destination_directory)
