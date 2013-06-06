#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

from functools import partial

import argparse
from getpass import getpass
import os
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../lib/python'))

"""

This script let you recv file from infinit.

You have to specify your user name in the INFINIT_USER env variable.

"""

def on_transaction(state, transaction, new):
    print("Transaction ({})".format(transaction),
          state.transaction_status(transaction))
    if state.transaction_status(transaction) == state.TransactionStatus.started:
        if getattr(state, "started_transactions", None):
            state.started_transactions.append(transaction)
        else:
            state.started_transactions = [transaction]
    elif state.transaction_status(transaction) == state.TransactionStatus.canceled:
        state.number_of_transactions -= 1
        if state.number_of_transactions == 0:
            state.running = False
    elif state.transaction_status(transaction) == state.TransactionStatus.finished:
        cnt = state.transaction_files_count(transaction)
        if cnt == 1:
            filename = state.transaction_first_filename(transaction)
            print("File received at '{}'".format(
                os.path.join(state.get_output_dir(), filename)))
        else:
            print("{} files received in '{}'".format(
                cnt, state.get_output_dir()))
        state.number_of_transactions -= 1
        if state.number_of_transactions == 0:
            state.running = False

def on_error(state, status, message, tid):
    if tid:
        print("Error ({}): {}: {}: {}".format(int(status), tid, status, message))
    else:
        print("Error ({}): {}: {}".format(int(status), status, message))
    state.running = False

def login(state, email = None):
    if not state.logged:
        receiver_id = os.getenv("INFINIT_USER", None)
        if receiver_id == None:
            raise Exception("you must provide INFINIT_USER")
        password = getpass("password: ")
        state.login(receiver_id, password)
    import socket
    state.set_device_name(socket.gethostname().strip())
    return state.email

def select_transactions(state, l_transactions, sender):
    if sender is not None:
        l_sender_id = state.search_users(sender)
        if not l_sender_id:
            raise Exception("no such user")
        if len(l_sender_id) > 1:
            raise Exception("ambiguous sender name")
        sender_id = l_sender_id[0]

    # select pending transactions matching the sender id (if not None)
    enumeration = list(enumerate(
            l for l in l_transactions
            if state.transaction_status(l) in [state.TransactionStatus.started, state.TransactionStatus.created, ]
            and (sender is None or state.transaction_sender_id(l) == sender_id)
    ))

    if len(enumeration) == 1:
        # if there is only one match, then return the id now
        return (enumeration[0][1],)

    # ask for user input
    for index, t in enumeration:
        first_filename  = state.transaction_first_filename(t)
        fullname        = state.transaction_sender_fullname(t)
        file_number     = state.transaction_files_count(t)
        if file_number > 1:
            print("[{}] {} files from {} ({})".format(index, file_number, fullname, t))
        else:
            print("[{}] {} from {} ({})".format(index, first_filename, fullname, t))

    selected = input("transaction numbers [all]> ")
    if selected:
        if isinstance(selected, (list, tuple)):
            l_selected = selected
        elif isinstance(selected, (str, bytes)):
            try:
                l_selected = (int(selected),)
            except ValueError as e:
                print(e)
                return []
        elif isinstance(selected, int):
            l_selected = (selected,)
    else:
        l_selected = []

    if l_selected:
        return (enumeration[int(i)][1] for i in l_selected)
    else:
        return l_transactions
    return []


def main(state, sender):
    id = login(state)

    # Pull only new notifications to ensure the transaction has been fetched.
    state.pull_notifications(0, 0)
    state.notifications_read()
    state.running = True
    transactions = state.transactions()

    state.on_error_callback(partial(on_error, state))
    state.transaction_callback(partial(on_transaction, state))

    if len(transactions) > 1:
        to_handle = list(select_transactions(state, transactions, sender))
    else:
        to_handle = list(transactions)

    if not to_handle:
        raise Exception("you must select a transaction to accept")
    state.number_of_transactions = len(to_handle)

    num = 0
    print(to_handle)
    for transaction_id in to_handle:
        print("accept transaction {}".format(transaction_id))
        state.current_transaction_id = transaction_id
        state.accept_transaction(transaction_id)

        while state.running:
            if getattr(state, "started_transactions", None):
                for t in (T for T in state.started_transactions if T in to_handle):
                    progress = state.transaction_progress(t)
                    print("Progress {2}: [{0:50s}] {1:.1f}% of {3}".format('#' * int(progress * 50), progress * 100, t, state.transaction_first_filename(t)), end=" "),
                    print("\r", end="")
            time.sleep(0.5)
            state.poll()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--sender", help="the user from whom you want to recv the file")
    parser.add_argument("-d", "--download_dir", help="the download dir")
    parser.add_argument("-l", "--logfile", help="path to the log file")
    args = parser.parse_args()

    if args.logfile:
        os.environ["INFINIT_LOG_FILE"] = args.logfile

    if args.download_dir:
        os.putenv("INFINIT_DOWNLOAD_DIR", args.download_dir)

    import gap
    with gap.State() as state:
        state.number_of_transactions = 0
        try:
            main(state, args.sender)
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
