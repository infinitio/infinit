#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils
def login(user_name, home, host, port):
    from getpass import getpass
    password = getpass("Identity password: ")

    from pycrust import User

    user = User(user_name, home, True)
    return User.store_token(user.login(password, host, port), user_name, home)

def main(args):
    from os import getenv
    home = args.infinit_home or getenv("INFINIT_HOME")
    if not home:
        raise Exception("You must provide a home directory, with --infinit-home or env INFINIT_HOME")
    print(login(args.user_name, home, args.meta_host, args.meta_port))

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.RemoteParser(require_token = False)

    parser.add_argument("--infinit-home",
                        help = "XXX: The path to your infinit home directory")
    parser.add_argument("--user-name",
                        help = "XXX: WILL BE REMOVE, IT'S YOU, IT'S YOUR NAME")

    # Parse arguments and handle errors.
    infinit_utils.run(parser, main)
