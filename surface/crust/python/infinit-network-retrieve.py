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
        meta_host, meta_port, meta_token_path = infinit_utils.meta_values(args)

        remote_retrieve(identifier = args.meta_network_identifier,
                        host = meta_host,
                        port = meta_port,
                        token_path = meta_token_path,
                        descriptor_path = args.store_local_descriptor_path)

    else:
        raise Exception("Neither --local-network-path nor --meta-network-identifier given")

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    # Remote.
    parser.add_argument("--meta-network-identifier",
                        help = "The identifier of the network")
    infinit_utils.meta_to_parser(parser)

    # Local.
    parser.add_argument("--local-network-path",
                        help = "XXX: The path to the network directory")

    # Storing.
    parser.add_argument("--store-local-descriptor-path",
                        help = "The path where the descriptor will be save.")
    parser.add_argument("--force",
                        action = 'store_true',
                        help = "Erase the file given with --store-local-descriptor-path if it already exists.")

    infinit_utils.run(parser, main)
