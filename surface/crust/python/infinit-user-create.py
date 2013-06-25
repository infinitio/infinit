#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def create(description, identity_path, force):
    from getpass import getpass
    password = getpass("Identity password: ")
    from pycrust import User

    user = User(password, description or "")

    if identity_path:
      user.store(identity_path, force or False)

    return user.identifier

def main(args):
    id = create(description = args.description,
                identity_path = args.store_local_identity_path,
                force = args.force)

    print(id)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--description",
                        help = "The description of this user")
    parser.add_argument("--store-local-identity-path",
                        help = "The path where identity file will be saved.")
    parser.add_argument("--force",
                        action = 'store_true',
                        help = "Erase the file given with --store-local-identity-path if it already exists.")

    from infinit_utils import run
    run(parser, main)
