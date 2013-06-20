#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def extract(user_name, network_name, home, descriptor_path):
    from pycrust import Network
    network = Network(user_name, network_name, home)

    if descriptor_path:
        network.store(descriptor_path)

def main(args):
    from os import path
    if not args.store_local_descriptor_path:
        raise Exception("You must provide a --store-local-descriptor-path.")
    if args.store_local_descriptor_path and not args.force and path.exists(args.store_local_descriptor_path):
        raise Exception("Descriptor storing destination already exists and you didn't specify --force option.")

    extract(args.user_name,
            args.network_name,
            args.infinit_home,
            args.store_local_descriptor_path)

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.LocalParser()
    parser.add_argument("network_name",
                        help = "XXX: The name you gave to the network")

    # Storing.
    parser.add_argument("--store-local-descriptor-path",
                        help = "The path where the descriptor will be save.")
    parser.add_argument("--force",
                        action = 'store_true',
                        help = "Erase the file given with --store-local-descriptor-path if it already exists.")

    infinit_utils.run(parser, main)
