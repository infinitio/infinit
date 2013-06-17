#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def create(name, identity_path, model, openness, policy, descriptor_path, force):
    from getpass import getpass
    password = getpass("Identity password: ")
    from pycrust import Network

    net = Network(name, identity_path, password, model, openness, policy)

    if descriptor_path:
      net.store(descriptor_path, force or False)

    return net.identifier

def main(args):
    from os import getenv, path
    identity_path = args.identity_path or getenv("INFINIT_IDENTITY_PATH")
    if not identity_path:
        raise Exception("You must provide --identity-path or INFINIT_IDENTITY_PATH")
    if args.store_local_descriptor_path and not args.force and path.exists(args.store_local_descriptor_path):
        raise Exception("Descriptor storing destination already exists and you didn't specify --force option.")

    id = create(name = args.NAME,
                identity_path = identity_path,
                model = args.model,
                openness = args.openness,
                policy = args.policy,
                descriptor_path = args.store_local_descriptor_path,
                force = args.force)

    print(id)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("NAME",
                        help = "The name of the network you want to create. It has to be unique for you.")
    parser.add_argument("--identity-path",
                        help = "The path to your identity file. You can also export INFINIT_IDENTITY_PATH.")
    parser.add_argument("--model",
                        choices = ['slug', 'local', 'remote', 'cirkle', 'kool'],
                        default = 'slug',
                        help = "The model of the network.")
    parser.add_argument("--policy",
                        choices = ['accessible', 'editable', 'confidential'],
                        default = 'accessible',
                        help = "The policy of the network.")
    parser.add_argument("--openness",
                        choices = ['open', 'community', 'closed'],
                        default = 'open',
                        help = "The openess of the network.")
    parser.add_argument("--store-local-descriptor-path",
                        help = "The path where the descriptor will be save.")
    parser.add_argument("--force",
                        action = 'store_true',
                        help = "Erase the file given with --store-local-descriptor-path if it already exists.")

    from infinit_utils import run
    run(parser, main)
