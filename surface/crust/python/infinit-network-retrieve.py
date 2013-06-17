#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def local_retrieve(path, descriptor_path):
    from pycrust import Network
    network = Network(path + "/descriptor")
    network.store(descriptor_path)

def remote_retrieve(identifier, host, port, token_path, descriptor_path):
    from pycrust import Network, ID
    network = Network(ID(identifier), host, port, token_path)

    network.store(descriptor_path)

def main(args):
    from os import path
    if not args.store_local_descriptor_path:
        raise Exception("You must provide a --store-local-descriptor-path")
    if args.store_local_descriptor_path and not args.force and path.exists(args.store_local_descriptor_path):
        raise Exception("Descriptor storing destination already exists and you didn't specify --force option.")

    if args.local_network_path:
        from os import path
        if not path.exists(args.local_network_path):
            raise Exception("Given path %s doesn't exist" % args.local_network_path)
        if args.remote_host or args.remote_port or args.remote_token_path:
            print("Warning: You provided both local and remote data. Local used.")
        local_retrieve(args.local_network_path, args.store_local_descriptor_path)

    elif args.remote_network_identifier:
        from os import getenv
        remote_host = args.remote_host or getenv("INFINIT_REMOTE_HOST")
        remote_port = args.remote_port or getenv("INFINIT_REMOTE_PORT")
        remote_token_path = args.remote_token_path or getenv("INFINIT_REMOTE_TOKEN_PATH")

        if not remote_host:
            raise Exception("You neither provided --remote-host nor exported INFINIT_REMOTE_HOST.")
        if not remote_port:
            raise Exception("You neither provided --remote-port nor exported INFINIT_REMOTE_PORT.")
        if not remote_token_path:
            raise Exception("You neither provided --remote-token-path nor exported INFINIT_REMOTE_TOKEN_PATH.")

        remote_retrieve(identifier = args.remote_network_identifier,
                        host = remote_host,
                        port = int(remote_port),
                        token_path = remote_token_path,
                        descriptor_path = args.store_local_descriptor_path)

    else:
        raise Exception("Neither --local-network-path nor --remote-network-identifier given")

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--remote-network-identifier",
                        help = "The identifier of the network")
    parser.add_argument("--remote-host",
                        help = "XXX: The host. You can also export INFINIT_REMOTE_HOST.")
    parser.add_argument("--remote-port",
                        type = int,
                        help = "XXX: The port. You can also export INFINIT_REMOTE_PORT.")
    parser.add_argument("--remote-token-path",
                        help = "XXX: The token path. You can also export INFINIT_REMOTE_TOKEN_PATH.")

    parser.add_argument("--local-network-path",
                        help = "XXX: The path to the network directory")

    parser.add_argument("--store-local-descriptor-path",
                        help = "The path where the descriptor will be save.")
    parser.add_argument("--force",
                        action = 'store_true',
                        help = "Erase the file given with --store-local-descriptor-path if it already exists.")

    import sys
    try:
        args = parser.parse_args()
        main(args)
        sys.exit(0)
    except Exception as e:
        print(e)
        sys.exit(1)
