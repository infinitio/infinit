#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

def destroy(identity_path):
    from pycrust import User
    User.erase(identity_path)

def main(args):
  destroy(args.LOCAL_IDENTITY_PATH)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("LOCAL_IDENTITY_PATH",
                        help = "The path to the identity to destroy.")

    from infinit_utils import run
    run(parser, main)
