#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import time
import argparse
import os

from pprint import pprint

def get_homedir(state):
    home = state.user_directory()
    return os.path.expanduser(home)

def write_file(path, token):
  with open(token_path, 'w+') as token_file:
    token_file.write("{}\n".format(token))

def login(state, email = None):
    from getpass import getpass
    receiver_id = os.getenv("INFINIT_USER", email)
    if receiver_id == None:
        raise Exception("you must provide an username")
    password = getpass("password: ")
    state.login(receiver_id, password)
    return state.generation_key()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-u", "--user", help="username")
    parser.add_argument("-l", "--logfile", help="path to the log file")
    args = parser.parse_args()

    if args.logfile:
        os.environ["INFINIT_LOG_FILE"] = args.logfile

    import gap
    with gap.State() as state:
        token = login(state, args.user) # you have to be logged in
        token_path = os.path.join(get_homedir(state), 'token')
        if not os.path.exists(token_path):
            write_file(token_path, token)
        print("export INFINIT_TOKEN_FILE={}".format(token_path))
