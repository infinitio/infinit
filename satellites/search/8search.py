#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import argparse
import os

from pprint import pprint

def login(state, email = None):
    from getpass import getpass
    receiver_id = os.getenv("INFINIT_USER", None)
    if receiver_id == None:
        raise Exception("you must provide INFINIT_USER")
    password = getpass("password: ")
    state.login(receiver_id, password)

def search_username(state, username):
    l_users = state.search_users(username)
    for U in l_users:
        s = dict()
        s["identifier"] = U
        s["fullname"] = state.user_fullname(U)
        s["email"] = state.user_handle(U)
        pprint(s)

def search_transactions(state):
    l_transactions = state.transactions()
    for T in l_transactions:
        s = dict()
        s["sender_id"] = state.transaction_sender_id(T)
        s["sender_fullname"] = state.transaction_sender_fullname(T)
        s["sender_device_id"] = state.transaction_sender_device_id(T)
        s["recipient_id"] = state.transaction_recipient_id(T)
        s["recipient_fullname"] = state.transaction_recipient_fullname(T)
        s["recipient_device_id"] = state.transaction_recipient_device_id(T)
        s["network_id"] = state.transaction_network_id(T)
        s["first_filename"] = state.transaction_first_filename(T)
        s["files_count"] = state.transaction_files_count(T)
        s["total_size"] = state.transaction_total_size(T)
        s["is_directory"] = state.transaction_is_directory(T)
        s["status"] = state.transaction_status(T)
        pprint(s)

def search_swaggers(state):
    l_swaggers = state.get_swaggers()
    for S in l_swaggers:
        s = dict()
        s["identifier"] = S
        s["fullname"] = state.user_fullname(S)
        s["email"] = state.user_email(S)
        pprint(s)

def search_me(state):
    email = os.getenv("INFINIT_USER")
    l_me = state.search_users(email)
    if len(l_me) > 1:
        raise Exception("Multiple match for {}".format(email))

    try:
        me = l_me[0]
    except IndexError:
        print("User", email ,"doesn't exits")
        return;
    s = dict()
    s["identifier"] = me
    s["fullname"] = state.user_fullname(me)
    s["email"] = state.user_email(me)
    pprint(s)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-u", "--username", help="an username you want to search")
    parser.add_argument("-t", "--transactions",
            help="list all the transactions",
            action="store_true")
    parser.add_argument("-s", "--swaggers",
            help="list all the swaggers",
            action="store_true")
    parser.add_argument("-m", "--me",
            help="get my info",
            action="store_true")
    parser.add_argument("-l", "--logfile", help="path to the log file")
    args = parser.parse_args()

    if args.logfile:
        os.environ["INFINIT_LOG_FILE"] = args.logfile

    import gap
    with gap.State() as state:
        if not state.logged:
            login(state, os.getenv("INFINIT_USER"))

        if args.username:
            search_username(state, args.username)

        if args.transactions:
            search_transactions(state)

        if args.swaggers:
            search_swaggers(state)

        if args.me:
            #search_me(state)
            print("not implemented")
