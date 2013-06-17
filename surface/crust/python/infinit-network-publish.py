#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def publish(descriptor_path, host, port, token_path):
    from pycrust import Network

    network = Network(descriptor_path)
    network.publish(host, port, token_path)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("LOCAL_DESCRIPTOR_PATH",
                        help = "The path to the descriptor to publish.")
    parser.add_argument("--remote-host",
                        help = "XXX: The host. You can also export INFINIT_REMOTE_HOST.")
    parser.add_argument("--remote-port",
                        type = int,
                        help = "XXX: The port. You can also export INFINIT_REMOTE_PORT.")
    parser.add_argument("--remote-token-path",
                        help = "XXX: The token path. You can also export INFINIT_REMOTE_TOKEN_PATH.")

    args = parser.parse_args()

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

    publish(descriptor_path = args.LOCAL_DESCRIPTOR_PATH,
            host = remote_host,
            port = int(remote_port),
            token_path = remote_token_path)
