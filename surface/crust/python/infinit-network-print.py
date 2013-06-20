#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils
from pycrust import Network

#
xml = """<network>
%s
</network>"""
xml_entry = "<%s>%s</%s>"

def to_xml(network, attributes):
    print(xml % "\n".join([xml_entry % (attr, getattr(network, attr), attr) for attr in attributes]))

json = """{
%s
}"""
json_entry = """{"%s": "%s"},"""

def to_json(network, attributes):
    print(json % "\n".join([json_entry % (attr, getattr(network, attr)) for attr in attributes]))

def to_csv(network, attributes):
    print(",".join(["%s" % getattr(network, attr) for attr in attributes]))

def to_raw(network, attributes):
    print("\n".join(["%s" % getattr(network, attr) for attr in attributes]))

formats = {"csv": to_csv, "json": to_json, "raw": to_raw, "xml": to_xml}

default_attributes = ('identifier', 'description')

def print_network(network, attributes, format_ = "raw"):
    formats[format_](network, attributes)

def main(args):
    attributes = args.attributes or []

    if args.local_descriptor_path:
        from os import path
        if args.local_descriptor_path:
            if not path.exists(args.local_descriptor_path):
                raise Exception("Given path %s for local_descriptor_path doesn't exist" % args.local_descriptor_path)
            print_network(Network(args.local_descriptor_path), attributes, args.format)
    else:
        from os import path
        home = args.infinit_home or getenv("INFINIT_HOME")
        if not home:
            raise Exception("You must provide a home directory, with --infinit-home or env INFINIT_HOME")
        if not args.network_name:
            raise Exception("You must provide a --network-name.")
        if not args.user_name:
            raise Exception("You must provide a --user-name, it's tempory.")

        print_network(Network(args.user_name, args.network_name, home),
                      attributes,
                      args.format)


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()

    # Home.
    parser.add_argument("--network-name",
                        help = "XXX:")
    parser.add_argument("--user-name",
                        help = "XXX:")
    parser.add_argument("--infinit-home",
                        help = "XXX:")

    # Direct.
    parser.add_argument("--local-descriptor-path",
                        help = "XXX:")

    # Formatting.
    parser.add_argument("--attributes",
                        nargs = '+',
                        choices = ['identifier', 'administrator_K', 'model',
                                   'everybody_identity', 'history', 'extent',
                                   'description', 'openness', 'policy', 'version'],
                        default = default_attributes,
                        help = "The list of attributes to display.")
    parser.add_argument("--format",
                        choices = ['csv', 'json', 'raw', 'xml'],
                        default = 'raw',
                        help = "The format to the print in.")

    infinit_utils.run(parser, main)
