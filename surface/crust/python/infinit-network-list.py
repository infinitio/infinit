#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def _list(user_name, home):
    from pycrust import Network
    return Network.list(user_name, home)

def main(args):
    print("\n".join(_list(args.user_name,
                          args.infinit_home)))

if __name__ == "__main__":
    import argparse

    parser = infinit_utils.LocalParser()
    infinit_utils.run(parser, main)
