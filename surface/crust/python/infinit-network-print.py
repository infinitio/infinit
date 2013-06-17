#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

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
        from os import getenv
        meta_host = args.meta_host or getenv("INFINIT_META_HOST")
        meta_port = args.meta_port or getenv("INFINIT_META_PORT")
        meta_token_path = args.meta_token_path or getenv("INFINIT_META_TOKEN_PATH")

        if not meta_host:
            raise Exception("You neither provided --meta-host nor exported INFINIT_META_HOST.")
        if not meta_port:
            raise Exception("You neither provided --meta-port nor exported INFINIT_META_PORT.")
        if not meta_token_path:
            raise Exception("You neither provided --meta-token-path nor exported INFINIT_META_TOKEN_PATH.")

        print_remote(id_ = args.meta_network_identifier,
                     host = meta_host,
                     port = int(meta_port),
                     token_path = meta_token_path,
                     attributes = attributes,
                     format_ = args.format)
    else:
        raise Exception("Neither --local-network-path, --local-descriptor-path nor --meta-network-identifier given")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--meta-network-identifier",
                        help = "XXX: The identifier of the network.")
    parser.add_argument("--meta-host",
                        help = "XXX: The host. You can also export INFINIT_META_HOST.")
    parser.add_argument("--meta-port",
                        type = int,
                        help = "XXX: The port. You can also export INFINIT_META_PORT.")
    parser.add_argument("--meta-token-path",
                        help = "XXX: The token path. You can also export INFINIT_META_TOKEN_PATH.")

    parser.add_argument("--local-network-path",
                        help = "XXX:")
    parser.add_argument("--local-descriptor-path",
                        help = "XXX:")

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

    from infinit_utils import run
    run(parser, main)
