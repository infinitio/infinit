#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def unpublish(host, port, token_path, identifier = None, name = None):
    from pycrust import Network, ID
    identifier = identifier or pycrust.lookup("", name, host, port, token_path)
    Network.unpublish(ID(identifier), host, port, token_path)

def main(args):
    if args.meta_network_identifier is None and args.meta_network_name is None:
        raise Exception("You must provide --infinit-network-identifier or --infinit-network-name")

    unpublish(identifier = args.meta_network_identifier,
              name = args.meta_network_name,
              host = args.meta_host,
              port = args.meta_port,
              token_path = args.meta_token_path)

if __name__ == "__main__":
    parser = infinit_utils.RemoteParser()
    parser.add_argument("--meta-network-identifier",
                        help = "The identifier of the network")
    parser.add_argument("--meta-network-name",
                        help = "The name of the network")

    infinit_utils.run(parser, main)
