#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def unpublish(identifier, host, port, token_path):
    from pycrust import Network, ID

    network = Network(ID(identifier), host, port, token_path)
    network.unpublish(host, port, token_path)

def main(args):
    meta_host, meta_port, meta_token_path = infinit_utils.meta_values(args)
    unpublish(identifier = args.META_NETWORK_IDENTIFIER,
              host = meta_host,
              port = meta_port,
              token_path = meta_token_path)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("META_NETWORK_IDENTIFIER",
                        help = "The identifier of the network")
    infinit_utils.meta_to_parser(parser)

    infinit_utils.run(parser, main)
