#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def fetch(host, port, token_path):
    from pycrust import Network
    return Network.fetch(host, port, token_path)

def main(args):
    print("\n".join(fetch(host = args.meta_host,
                          port = args.meta_port,
                          token_path = args.meta_token_path)))

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.RemoteParser()
    infinit_utils.run(parser, main)
