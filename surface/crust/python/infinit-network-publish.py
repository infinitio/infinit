#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def publish(descriptor_path, host, port, token_path):
    from pycrust import Network

    network = Network(descriptor_path)
    network.publish(host, port, token_path)

def main(args):
    meta_host, meta_port, meta_token_path = infinit_utils.meta_values(args)

    publish(descriptor_path = args.LOCAL_DESCRIPTOR_PATH,
            host = meta_host,
            port = meta_port,
            token_path = meta_token_path)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("LOCAL_DESCRIPTOR_PATH",
                        help = "The path to the descriptor to publish.")
    infinit_utils.meta_to_parser(parser)

    # Parse arguments and handle errors.
    infinit_utils.run(parser, main)
