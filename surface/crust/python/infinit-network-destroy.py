#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def destroy(descriptor_path):
    from pycrust import Network
    net = Network(descriptor_path)
    net.erase()

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()

    # Destroy
    parser.add_argument("LOCAL_DESCRIPTOR_PATH",
                        help="The path to the descriptor to destroy.")

    args = parser.parse_args()
    destroy(args.LOCAL_DESCRIPTOR_PATH)
