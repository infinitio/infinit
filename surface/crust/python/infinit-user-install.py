#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def install(identity_path, user_name, home):
    from pycrust import User
    net = User(identity_path)
    net.install(user_name, home)

def main(args):
    if not args.user_name:
        raise Exception("You must provide a --user-name.")

    install(identity_path = args.LOCAL_IDENTITY_PATH,
            user_name = args.user_name,
            home = args.infinit_home)

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.LocalParser()
    parser.add_argument("LOCAL_IDENTITY_PATH",
                        help = "The path to the descriptor to destroy.")

    infinit_utils.run(parser, main)
