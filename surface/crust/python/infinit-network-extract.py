#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def local_retrieve(user_name, network_name, home, descriptor_path):
    from pycrust import Network
    network = Network(user_name, network_name, home)

    if descriptor_path:
        network.store(descriptor_path)

def main(args):
    from os import path
    home = args.infinit_home or getenv("INFINIT_HOME")
    if not home:
        raise Exception("You must provide a home directory, with --infinit-home or env INFINIT_HOME")
    if not args.user_name:
        raise Exception("You must provide a --user-name, it's tempory.")
    if not args.store_local_descriptor_path:
        raise Exception("You must provide a --store-local-descriptor-path.")
    if args.store_local_descriptor_path and not args.force and path.exists(args.store_local_descriptor_path):
        raise Exception("Descriptor storing destination already exists and you didn't specify --force option.")

    local_retrieve(args.user_name,
                   args.network_name,
                   args.infinit_home,
                   args.store_local_descriptor_path)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    # Local data.
    parser.add_argument("network_name",
                        help = "XXX: The name you gave to the network")
    parser.add_argument("--infinit-home",
                        help = "XXX: The path to your infinit home directory")
    parser.add_argument("--user-name",
                        help = "XXX: WILL BE REMOVE, IT'S YOU, IT'S YOUR NAME")

    # Storing.
    parser.add_argument("--store-local-descriptor-path",
                        help = "The path where the descriptor will be save.")
    parser.add_argument("--force",
                        action = 'store_true',
                        help = "Erase the file given with --store-local-descriptor-path if it already exists.")

    infinit_utils.run(parser, main)
