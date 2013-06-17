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
        if args.remote_host or args.remote_port or args.remote_token_path:
            print("Warning: You provided both local and remote data. Local used.")
        print(local_list(args.local_repository))
    else:
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

        print(remote_list(host = remote_host,
                          port = int(remote_port),
                          token_path = remote_token_path))

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--remote-host",
                        help = "XXX: The host. You can also export INFINIT_REMOTE_HOST.")
    parser.add_argument("--remote-port",
                        type = int,
                        help = "XXX: The port. You can also export INFINIT_REMOTE_PORT.")
    parser.add_argument("--remote-token-path",
                        help = "XXX: The token path. You can also export INFINIT_REMOTE_TOKEN_PATH.")
    parser.add_argument("--local-repository",
                        help = "XXX: The path where you use to store your networks.")

    import sys
    try:
        args = parser.parse_args()
        main(args)
        sys.exit(0)
    except Exception as e:
        print(e)
        sys.exit(1)
