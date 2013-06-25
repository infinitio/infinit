#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def unpublish(host, port, token_path, name = None):
    from pycrust import Network
    Network.unpublish(name, host, port, token_path)

def main(args):
    unpublish(name = args.META_NETWORK_NAME,
              host = args.meta_host,
              port = args.meta_port,
              token_path = args.meta_token_path)

if __name__ == "__main__":
    parser = infinit_utils.RemoteParser()
    parser.add_argument("META_NETWORK_NAME",
                        help = "The name of the network")

    infinit_utils.run(parser, main)
