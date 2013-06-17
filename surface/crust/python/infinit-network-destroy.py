#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def destroy(descriptor_path):
    from pycrust import Network
    net = Network(descriptor_path)
    net.erase()

def main(args):
    destroy(args.LOCAL_DESCRIPTOR_PATH)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("LOCAL_DESCRIPTOR_PATH",
                        help = "The path to the descriptor to destroy.")

    import sys
    try:
        args = parser.parse_args()
        main(args)
        sys.exit(0)
    except Exception as e:
        print(str(e).capitalize())
        sys.exit(1)
