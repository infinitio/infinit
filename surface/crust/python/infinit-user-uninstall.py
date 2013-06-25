#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

def uninstall(user_name, home):
    from pycrust import User
    User.uninstall(user_name, home)

def main(args):
    uninstall(args.user_name,
              args.infinit_home),

if __name__ == "__main__":
    parser = infinit_utils.LocalParser()
    parser.add_argument("user_name",
                        help = "XXX: The name you gave to the network")

    infinit_utils.run(parser, main)
