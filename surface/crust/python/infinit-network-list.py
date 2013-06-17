#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

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

        print("\n".join(remote_list(host = meta_host,
                                    port = int(meta_port),
                                    token_path = meta_token_path)))

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--meta-host",
                        help = "XXX: The host. You can also export INFINIT_META_HOST.")
    parser.add_argument("--meta-port",
                        type = int,
                        help = "XXX: The port. You can also export INFINIT_META_PORT.")
    parser.add_argument("--meta-token-path",
                        help = "XXX: The token path. You can also export INFINIT_META_TOKEN_PATH.")
    parser.add_argument("--local-repository",
                        help = "XXX: The path where you use to store your networks.")

    from infinit_utils import run
    run(parser, main)
