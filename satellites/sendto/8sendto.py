#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import argparse
import time
import sys
import os

from functools import partial
from getpass import getpass

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../lib/python'))

"""

This script let you send files through infinit.

You have to specify your user name in the INFINIT_USER env variable.

This script blocks until the end of the transfer.

"""

def login(state, email = None):
    sender_id = os.getenv("INFINIT_USER", None)
    if sender_id == None:
        raise Exception("you must provide INFINIT_USER")

    password = getpass("password: ")
    state.login(sender_id, password)
    import socket
    state.set_device_name(socket.gethostname().strip())
    state.connect()

def debug_status(state, transaction, new):
    print("trasaction status changed to", state.transaction_status(transaction))

def on_started(state, transaction, new):
    state.current_transaction_id = transaction

def on_canceled(state, transaction, new):
    if state.transaction_status(transaction) == state.TransactionStatus.canceled:
        print("Transaction canceled, check your log")
        state.running = False

def on_finished(state, transaction, new):
    if state.transaction_status(transaction) == state.TransactionStatus.finished:
        print("Transaction succeeded")
        state.running = False

def main(state, user, filepath):
    login(state)

    id = state.send_files(user, [filepath,])
    state.transaction_callback(partial(on_started, state))
    state.transaction_status_callback(partial(on_finished, state))
    state.transaction_status_callback(partial(on_canceled, state))
    state.transaction_status_callback(partial(debug_status, state))
    state.running = True

    while True:
        time.sleep(0.5)
        status = state.operation_status(id)
        if status == state.OperationStatus.running:
            print(".", end="", file=sys.stdout)
            sys.stdout.flush()
        if status == state.OperationStatus.success:
            print("Preparation finished, waiting for receiver")
            break
        if status == state.OperationStatus.failure:
            print("Failure to prepare the transfer.")
            return

    while state.running:
        time.sleep(0.5)
        state.poll()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("user", help="to user to who you want to send a file")
    parser.add_argument("file", help="path to the file")
    parser.add_argument("-l", "--logfile", help="path to the log file")
    args = parser.parse_args()

    if args.logfile:
        os.environ["INFINIT_LOG_FILE"] = args.logfile

    import gap
    state = gap.State()

    try:
        main(state, args.user, args.file)
    except KeyboardInterrupt as e:
        if getattr(state, "current_transaction_id", None):
            tid = state.current_transaction_id
            print("Interupted.")
            print("Cancel the outgoing transaction ({})".format(tid))
            state.update_transaction(tid, state.TransactionStatus.canceled)
    except Exception as e:
        if getattr(state, "current_transaction_id", None):
            tid = state.current_transaction_id
            print("Interupted. Cancel the outgoing transaction ({})".format(tid))
            state.update_transaction(tid, state.TransactionStatus.canceled)
        raise e
    del state
