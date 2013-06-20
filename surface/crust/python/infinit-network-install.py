#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def install(descriptor_path, network_name, user_name, home):
    from pycrust import Network
    net = Network(descriptor_path)
    net.install(user_name, network_name, home)

def main(args):
    if not args.network_name:
        raise Exception("You must provide a --network-name.")

    install(descriptor_path = args.LOCAL_DESCRIPTOR_PATH,
            network_name = args.network_name,
            user_name = args.user_name,
            home = args.infinit_home)

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.LocalParser()
    parser.add_argument("LOCAL_DESCRIPTOR_PATH",
                        help = "The path to the descriptor to destroy.")
    parser.add_argument("--network-name",
                        help = "XXX: The name you gave to the network")

    infinit_utils.run(parser, main)
