#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def retrieve(owner_name, network_name, host, port, token_path, descriptor_path):
    from pycrust import Network
    network = Network(owner_name, network_name, host, port, token_path)

    if descriptor_path:
        network.store(descriptor_path)

def main(args):
    from os import path
    if not args.store_local_descriptor_path:
        raise Exception("You must provide a --store-local-descriptor-path.")
    if args.store_local_descriptor_path and not args.force and path.exists(args.store_local_descriptor_path):
        raise Exception("Descriptor storing destination already exists and you didn't specify --force option.")

    retrieve(owner_name = args.owner_name or "",
             network_name = args.network_name,
             host = args.meta_host,
             port = args.meta_port,
             token_path = args.meta_token_path,
             descriptor_path = args.store_local_descriptor_path)

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.RemoteParser()
    parser.add_argument("network_name",
                        help = "XXX: The name you gave to the network")
    parser.add_argument("--owner-name",
                        help = "XXX: The owner of the network. If not provided, it's you.")


    parser.add_argument("--store-local-descriptor-path",
                        help = "The path where the descriptor will be save.")
    parser.add_argument("--force",
                        action = 'store_true',
                        help = "Erase the file given with --store-local-descriptor-path if it already exists.")

    infinit_utils.run(parser, main)
