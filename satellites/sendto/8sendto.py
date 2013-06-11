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
    if not state.logged:
        sender_id = os.getenv("INFINIT_USER", None)
        if sender_id == None:
            raise Exception("you must provide INFINIT_USER")

        password = getpass("password: ")
        state.login(sender_id, password)
    import socket
    state.set_device_name(socket.gethostname().strip())

def on_transaction(state, transaction, new):
    status = state.transaction_status(transaction)
    if new:
        print("New transaction", transaction)
    else:
        print("Transaction ({})".format(transaction), status)
    state.current_transaction_id = transaction
    if status in (
       state.TransactionStatus.canceled,
       state.TransactionStatus.finished,
       state.TransactionStatus.failed,
    ):
        state.running = False
    elif state.transaction_accepted(transaction):
        state.started = True

def on_error(state, status, message, tid):
    if tid:
        print("Error ({}): {}: {}: {}".format(int(status), tid, status, message))
    else:
        print("Error ({}): {}: {}".format(int(status), status, message))
    state.running = False

def main(state, user, files):

    id = state.send_files(user, files)

    state.transaction_callback(partial(on_transaction, state))
    state.on_error_callback(partial(on_error, state))
    state.running = True
    state.started = False
    state.current_transaction_id = None

    while state.running:
        if state.current_transaction_id is not None and state.started:
            tid = state.current_transaction_id
            progress = state.transaction_progress(tid)
            filename = state.transaction_first_filename(tid)
            print(
               "\rProgress {2}: [{0:50s}] {1:.1f}% of {3}".format(
                   '#' * int(progress * 50), progress * 100,
                   tid, filename
               ),
               end=""
            )
        time.sleep(1)
        state.poll()

def go(state, user, files):
    try:
        main(state, user, files)
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

def get_homedir():
    home = os.getenv("INFINIT_HOME", "~/.infinit/")
    return os.path.expanduser(home)

def login_and_go(args):
    with gap.State() as state:
        login(state, args.user)
        go(state, args.user, args.files)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("user", help="to user to who you want to send a file")
    parser.add_argument("files", nargs='+', help="path to the file")
    parser.add_argument("-l", "--logfile", help="path to the log file")
    args = parser.parse_args()

    if args.logfile:
        os.environ["INFINIT_LOG_FILE"] = args.logfile

    import gap
    login_and_go(args)
