#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def publish(descriptor_path, network_name, host, port, token_path):
    from pycrust import Network

    network = Network(descriptor_path)
    network.publish(network_name, host, port, token_path)

def main(args):
    if not args.network_name:
        raise Exception("You must provide a --network-name.")

    publish(descriptor_path = args.LOCAL_DESCRIPTOR_PATH,
            network_name = args.network_name,
            host = args.meta_host,
            port = args.meta_port,
            token_path = args.meta_token_path)

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.RemoteParser()
    parser.add_argument("LOCAL_DESCRIPTOR_PATH",
                        help = "The path to the descriptor to publish.")
    parser.add_argument("--network-name",
                        help = "The name as which you want to store this network.")

    # Parse arguments and handle errors.
    infinit_utils.run(parser, main)
