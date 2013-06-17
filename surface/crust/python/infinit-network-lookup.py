#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def lookup(owner_handle, network_name, host, port, token_path):
    from pycrust import Network
    return Network.lookup(owner_handle, network_name, host, port, token_path)

def main(args):
    meta_host, meta_port, meta_token_path = infinit_utils.meta_values(args)

    id = lookup(owner_handle = args.NETWORK_OWNER,
                network_name = args.NETWORK_NAME,
                host = meta_host,
                port = meta_port,
                token_path = meta_token_path)

    print(id)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("NETWORK_OWNER",
                        help = "The handle of the owner.")
    parser.add_argument("NETWORK_NAME",
                        help = "The name of the network.")
    infinit_utils.meta_to_parser(parser)

    infinit_utils.run(parser, main)
