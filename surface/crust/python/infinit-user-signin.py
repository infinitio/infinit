#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def signin(identity_path, user_name, host, port):
    from pycrust import User

    user = User(identity_path)
    user.signin(user_name, host, port)

def main(args):
    signin(identity_path = args.LOCAL_IDENTITY_PATH,
           user_name = args.meta_user_name,
           host = args.meta_host,
           port = args.meta_port)

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.RemoteParser(require_token = False)
    parser.add_argument("LOCAL_IDENTITY_PATH",
                        help = "The path to the identity to publish.")

    parser.add_argument("--meta-user-name",
                        help = "The name meta will reference you.")

    # Parse arguments and handle errors.
    infinit_utils.run(parser, main)
