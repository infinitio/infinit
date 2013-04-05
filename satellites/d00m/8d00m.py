#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

from functools import partial
from getpass import getpass

import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../lib/python'))

"""

This script let you recv file from infinit.

You have to specify your user name in the INFINIT_USER env variable.

"""
def on_canceled(state, transaction, new):
    if state.transaction_status(transaction) == state.TransactionStatus.canceled:
        print("Transaction {} canceled".format(transaction))
        state.number_of_transactions -= 1
        if state.number_of_transactions == 0:
            state.running = False

def login(state, email = None):
    receiver_id = os.getenv("INFINIT_USER", None)
    if receiver_id == None:
        raise Exception("you must provide INFINIT_USER")
    password = getpass("password: ")
    state.login(receiver_id, password)
    state.connect()
    return receiver_id


def main(state):
    id = login(state)

    state.transaction_status_callback(partial(on_canceled, state))

    state.running = True
    state.set_device_name(id + "device")

    transactions = state.transactions()
    state.number_of_transactions = len(transactions)

    for transaction_id in transactions:
        print("doom transaction {}".format(transaction_id))
        state.update_transaction(transaction_id, state.TransactionStatus.canceled)
    else:
        return

    while state.running:
        state.poll()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--logfile", help="path to the log file")
    args = parser.parse_args()

    if args.logfile:
        os.environ["INFINIT_LOG_FILE"] = args.logfile

    import gap
    state = gap.State()

    try:
        main(state)
    except KeyboardInterrupt as e:
        if getattr(state, "current_transaction_id", None):
            tid = state.current_transaction_id
            print("Interupted.")
    del state
