#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def lookup(owner_handle, network_name, host, port):
    from pycrust import Network
    return Network.lookup(owner_handle, network_name, host, port)

def main(args):
    from os import getenv
    meta_host = args.meta_host or getenv("INFINIT_META_HOST")
    meta_port = args.meta_port or getenv("INFINIT_META_PORT")
    # meta_token_path = args.meta_token_path or getenv("INFINIT_META_TOKEN_PATH")

    if not meta_host:
        raise Exception("You didn't provide meta host.")
    if not meta_port:
        raise Exception("You didn't provide meta port.")

    id = lookup(owner_handle = args.NETWORK_OWNER,
                network_name = args.NETWORK_NAME,
                host = meta_host,
                port = int(meta_port))

    print(id)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("NETWORK_OWNER",
                        help = "The handle of the owner.")
    parser.add_argument("NETWORK_NAME",
                        help = "The name of the network.")
    parser.add_argument("--meta-host",
                        type = int,
                        help = "XXX: The host. You can also export INFINIT_META_HOST.")
    parser.add_argument("--meta-port",
                        help = "XXX: The port. You can also export INFINIT_META_PORT.")
    # parser.add_argument("--meta-token-path",
    #                     help = "XXX: The token path. You can also export INFINIT_META_TOKEN_PATH.")

    from infinit_utils import run
    run(parser, main)
