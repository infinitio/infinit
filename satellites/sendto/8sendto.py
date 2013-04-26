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

def show_status(state, transaction, new):
    print("Transaction ({})".format(transaction), state.transaction_status(transaction))

def on_transaction(state, transaction, new):
    print("New transaction", transaction)
    state.current_transaction_id = transaction

def on_started(state, transaction, new):
    if state.transaction_status(transaction) == state.TransactionStatus.started:
        state.started = True

def on_canceled(state, transaction, new):
    if state.transaction_status(transaction) == state.TransactionStatus.canceled:
        state.running = False

def on_finished(state, transaction, new):
    if state.transaction_status(transaction) == state.TransactionStatus.finished:
        state.running = False

def on_error(state, status, message, tid):
    if tid:
        print("Error ({}): {}: {}: {}".format(int(status), tid, status, message))
    else:
        print("Error ({}): {}: {}".format(int(status), status, message))
    state.running = False

def main(state, user, files):
    login(state)

    id = state.send_files(user, files)
    state.transaction_callback(partial(on_transaction, state))
    state.transaction_status_callback(partial(on_finished, state))
    state.transaction_status_callback(partial(on_started, state))
    state.transaction_status_callback(partial(on_canceled, state))
    state.transaction_status_callback(partial(show_status, state))
    state.on_error_callback(partial(on_error, state))
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
        if getattr(state, "current_transaction_id", None) and getattr(state, "started", None):
            tid = state.current_transaction_id
            progress = state.transaction_progress(tid)
            print("Progress {2}: [{0:50s}] {1:.1f}% of {3}".format('#' * int(progress * 50), progress * 100, tid, state.transaction_first_filename(tid)), end=" "),
            print("\r", end="")
        time.sleep(1)
        state.poll()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("user", help="to user to who you want to send a file")
    parser.add_argument("files", nargs='+', help="path to the file")
    parser.add_argument("-l", "--logfile", help="path to the log file")
    args = parser.parse_args()

    if args.logfile:
        os.environ["INFINIT_LOG_FILE"] = args.logfile

    import gap
    with gap.State() as state:
        try:
            main(state, args.user, args.files)
        except KeyboardInterrupt as e:
            if getattr(state, "current_transaction_id", None):
                tid = state.current_transaction_id
                print("Interrupted. Cancel the outgoing transaction ({})".format(tid))
                state.update_transaction(tid, state.TransactionStatus.canceled)
        except Exception as e:
            if getattr(state, "current_transaction_id", None):
                tid = state.current_transaction_id
                print("Interrupted. Cancel the outgoing transaction ({})".format(tid))
                state.update_transaction(tid, state.TransactionStatus.canceled)
            raise e
