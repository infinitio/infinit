#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def lookup(owner_handle, network_name, host, port, token_path):
    from pycrust import Network
    return Network.lookup(owner_handle, network_name, host, port, token_path)

def main(args):
    id = lookup(owner_handle = args.network_owner or "",
                network_name = args.NETWORK_NAME,
                host = args.meta_host,
                port = args.meta_port,
                token_path = args.meta_token_path)

    print(id)

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.RemoteParser()
    parser.add_argument("--network_owner",
                        help = "The handle of the owner.")
    parser.add_argument("NETWORK_NAME",
                        help = "The name of the network.")

    infinit_utils.run(parser, main)
