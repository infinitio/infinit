#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def uninstall(network_path):
    from pycrust import Network
    net = Network(network_path + "/descriptor")
    net.uninstall(network_path)

def main(args):
    from os import path
    if not path.exists(args.LOCAL_NETWORK_PATH):
        raise Exception("Network path %s doesn't exist" % args.LOCAL_NETWORK_PATH)
    if not path.exists(args.LOCAL_NETWORK_PATH + "/descriptor"):
        raise Exception("The network doesn't contains descriptor file. Use rm -rf.")

    uninstall(args.LOCAL_NETWORK_PATH)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("LOCAL_NETWORK_PATH",
                        help = "The path to the network to destroy.")

    from infinit_utils import run
    run(parser, main)
