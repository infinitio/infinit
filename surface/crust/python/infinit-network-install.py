#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def install(descriptor_path, network_path):
    from pycrust import Network
    net = Network(descriptor_path)
    net.install(network_path)

def main(args):
    if not args.infinit_network_path:
        raise Exception("You must provide --infinit-network-path")
    from os import path
    if path.exists(args.infinit_network_path):
        raise Exception("Destination path %s provided via --infinit-network-path already exists" % args.infinit_network_path)

    install(descriptor_path = args.LOCAL_DESCRIPTOR_PATH,
            network_path = args.infinit_network_path)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("LOCAL_DESCRIPTOR_PATH",
                        help = "The path to the descriptor to destroy.")
    parser.add_argument("--infinit-network-path",
                        help = "The path where install your network.")

    import sys
    try:
        args = parser.parse_args()
        main(args)
        sys.exit(0)
    except Exception as e:
        print(e)
        sys.exit(1)
