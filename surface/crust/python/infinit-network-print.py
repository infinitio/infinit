#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

import infinit_utils

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

default_attributes = ('identifier', 'name')

def print_network(network, attributes, format_):
    formats[format_](network, attributes)

def print_local(path, attributes=default_attributes, format_ = "raw"):
    from pycrust import Network
    network = Network(path)
    print_network(network, attributes, format_)

def print_remote(id_, host, port, token_path, attributes = default_attributes, format_= "raw"):
    from pycrust import Network, ID
    network = Network(ID(id_), host, port, token_path)
    print_network(network, attributes, format_)

def main(args):
    attributes = args.attributes or []

    if args.local_network_path or args.local_descriptor_path:
        if args.meta_host or args.meta_port or args.meta_token_path:
            print("Warning: You provided both local and meta data. Local used.")
        from os import path
        if args.local_descriptor_path:
            if not path.exists(args.local_descriptor_path):
                raise Exception("Given path %s for local_descriptor_path doesn't exist" % args.local_descriptor_path)
            if args.local_network_path:
                print("WARNING: You set both --local_descriptor_path and --local_network_path. Descriptor taken by default")
            print_local(args.local_descriptor_path,
                        attributes,
                        args.format)
        else:
            if not path.exists(args.local_network_path):
                raise Exception("Given path %s for --local_network_path doesn't exist" % args.local_repository)
            print_local(args.local_network_path + "/descriptor",
                        attributes,
                        args.format)
    elif args.meta_network_identifier:
        meta_host, meta_port, meta_token_path = infinit_utils.meta_values(args)

        print_remote(id_ = args.meta_network_identifier,
                     host = meta_host,
                     port = meta_port,
                     token_path = meta_token_path,
                     attributes = attributes,
                     format_ = args.format)
    else:
        raise Exception("Neither --local-network-path, --local-descriptor-path nor --meta-network-identifier given")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    # Remote.
    parser.add_argument("--meta-network-identifier",
                        help = "XXX: The identifier of the network.")
    infinit_utils.meta_to_parser(parser)

    # Local.
    parser.add_argument("--local-network-path",
                        help = "XXX:")
    parser.add_argument("--local-descriptor-path",
                        help = "XXX:")

    # Formatting.
    parser.add_argument("--attributes",
                        nargs = '+',
                        choices = ['identifier', 'administrator_K', 'model',
                                   'everybody_identity', 'history', 'extent',
                                   'name', 'openness', 'policy', 'version'],
                        default = default_attributes,
                        help = "The list of attributes to display.")
    parser.add_argument("--format",
                        choices = ['csv', 'json', 'raw', 'xml'],
                        default = 'raw',
                        help = "The format to the print in.")

    infinit_utils.run(parser, main)
