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
    if not state.logged:
        if email is None:
            email = os.environ.get("INFINIT_USER")
        if email is None:
            email = input("email: ").strip()
        else:
            print("email:", email)
        password = getpass("password: ")
        state.login(email, password)
    import socket
    state.set_device_name(socket.gethostname().strip())
    return email


def main(state, email):
    email = login(state, email)

    state.running = True

    state.pull_notifications(0, 0)
    transactions = state.transactions()
    state.number_of_transactions = len(transactions)
    state.transaction_status_callback(partial(on_canceled, state))

    for transaction_id in transactions:
        tid = transaction_id
        try:
            if not (state.transaction_status(tid) == state.TransactionStatus.canceled) \
               and not (state.transaction_status(tid) == state.TransactionStatus.finished):
                print("* doomed transaction {}".format(transaction_id))
                state.update_transaction(tid, state.TransactionStatus.canceled)
        except Exception as e:
            print(tid, e)
    else:
        return

    networks = state.networks()
    for network in networks:
        print(network)
        state.delete_network(network)
    while state.running:
        state.poll()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(sys.argv[0], description="Clean user's transactions and networks")
    parser.add_argument("-l", "--logfile", help="path to the log file")
    parser.add_argument(
        '-u', '--user',
        help = "Specify user to clean",
        action = "store",
        default = os.environ.get("INFINIT_USER"),
    )
    parser.add_argument('-n', '--networks', help="Destroy all user's networks")
    args = parser.parse_args()

    if args.logfile:
        os.environ["INFINIT_LOG_FILE"] = args.logfile

    import gap
    with gap.State() as state:
        try:
            main(state, args.user)
        except KeyboardInterrupt as e:
            if getattr(state, "current_transaction_id", None):
                tid = state.current_transaction_id
                print("Interrupted.")
