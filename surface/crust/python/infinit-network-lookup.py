#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def lookup(owner_handle, network_name, host, port):
    from pycrust import Network
    return Network.lookup(owner_handle, network_name, host, port)

def main(args):
    from os import getenv
    remote_host = args.remote_host or getenv("INFINIT_REMOTE_HOST")
    remote_port = args.remote_port or getenv("INFINIT_REMOTE_PORT")
    # remote_token_path = args.remote_token_path or getenv("INFINIT_REMOTE_TOKEN_PATH")

    if not remote_host:
        raise Exception("You didn't provide remote host.")
    if not remote_port:
        raise Exception("You didn't provide remote port.")

    id = lookup(owner_handle = args.NETWORK_OWNER,
                network_name = args.NETWORK_NAME,
                host = remote_host,
                port = int(remote_port))

    print(id)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("NETWORK_OWNER",
                        help = "The handle of the owner.")
    parser.add_argument("NETWORK_NAME",
                        help = "The name of the network.")
    parser.add_argument("--remote-host",
                        type = int,
                        help = "XXX: The host. You can also export INFINIT_REMOTE_HOST.")
    parser.add_argument("--remote-port",
                        help = "XXX: The port. You can also export INFINIT_REMOTE_PORT.")
    # parser.add_argument("--remote-token-path",
    #                     help = "XXX: The token path. You can also export INFINIT_REMOTE_TOKEN_PATH.")

    import sys
    try:
        args = parser.parse_args()
        main(args)
        sys.exit(0)
    except Exception as e:
        print(e)
        sys.exit(1)
