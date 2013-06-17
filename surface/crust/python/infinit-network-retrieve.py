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
        if args.meta_host or args.meta_port or args.meta_token_path:
            print("Warning: You provided both local and meta data. Local used.")
        local_retrieve(args.local_network_path, args.store_local_descriptor_path)

    elif args.meta_network_identifier:
        from os import getenv
        meta_host = args.meta_host or getenv("INFINIT_META_HOST")
        meta_port = args.meta_port or getenv("INFINIT_META_PORT")
        meta_token_path = args.meta_token_path or getenv("INFINIT_META_TOKEN_PATH")

        if not meta_host:
            raise Exception("You neither provided --meta-host nor exported INFINIT_META_HOST.")
        if not meta_port:
            raise Exception("You neither provided --meta-port nor exported INFINIT_META_PORT.")
        if not meta_token_path:
            raise Exception("You neither provided --meta-token-path nor exported INFINIT_META_TOKEN_PATH.")

        remote_retrieve(identifier = args.meta_network_identifier,
                        host = meta_host,
                        port = int(meta_port),
                        token_path = meta_token_path,
                        descriptor_path = args.store_local_descriptor_path)

    else:
        raise Exception("Neither --local-network-path nor --meta-network-identifier given")

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--meta-network-identifier",
                        help = "The identifier of the network")
    parser.add_argument("--meta-host",
                        help = "XXX: The host. You can also export INFINIT_META_HOST.")
    parser.add_argument("--meta-port",
                        type = int,
                        help = "XXX: The port. You can also export INFINIT_META_PORT.")
    parser.add_argument("--meta-token-path",
                        help = "XXX: The token path. You can also export INFINIT_META_TOKEN_PATH.")

    parser.add_argument("--local-network-path",
                        help = "XXX: The path to the network directory")

    parser.add_argument("--store-local-descriptor-path",
                        help = "The path where the descriptor will be save.")
    parser.add_argument("--force",
                        action = 'store_true',
                        help = "Erase the file given with --store-local-descriptor-path if it already exists.")

    from infinit_utils import run
    run(parser, main)
