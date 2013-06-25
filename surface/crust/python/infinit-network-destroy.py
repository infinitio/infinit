#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def destroy(descriptor_path):
    from pycrust import Network
    Network.erase(descriptor_path)

def main(args):
    destroy(args.LOCAL_DESCRIPTOR_PATH)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("LOCAL_DESCRIPTOR_PATH",
                        help = "The path to the descriptor to destroy.")

    from infinit_utils import run
    run(parser, main)
