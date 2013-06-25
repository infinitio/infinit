#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def signout(host, port, token):
    from pycrust import User
    User.signout(host, port, token)

def main(args):
    signout(host = args.meta_host,
            port = args.meta_port,
            token = args.meta_token_path)

if __name__ == "__main__":
    import argparse
    parser = infinit_utils.RemoteParser()

    # Parse arguments and handle errors.
    infinit_utils.run(parser, main)
