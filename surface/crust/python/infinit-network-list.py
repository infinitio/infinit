#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def local_list(path):
    from pycrust import Network
    return Network.local_list(path)

def remote_list(host, port, token_path):
    from pycrust import Network
    return Network.remote_list(host, port, token_path)

def main(args):
    if args.local_repository:
        from os import path
        if not path.exists(args.local_repository):
            raise Exception("Given path %s doesn't exist" % args.local_repository)
        if args.meta_host or args.meta_port or args.meta_token_path:
            print("Warning: You provided both local and meta data. Local used.")
        print("\n".join(local_list(args.local_repository)))
    else:
        meta_host, meta_port, meta_token_path = infinit_utils.meta_values(args)
        print("\n".join(remote_list(host = meta_host,
                                    port = int(meta_port),
                                    token_path = meta_token_path)))

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    # Remote.
    infinit_utils.meta_to_parser(parser)

    # Local.
    parser.add_argument("--local-repository",
                        help = "XXX: The path where you use to store your networks.")

    infinit_utils.run(parser, main)
