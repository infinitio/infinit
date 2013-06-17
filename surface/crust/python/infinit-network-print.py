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

def print_local(path, attributes=default_attributes, format_ = "bite"):
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
        if args.remote_host or args.remote_port or args.remote_token_path:
            print("Warning: You provided both local and remote data. Local used.")
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
    elif args.remote_network_identifier:
        from os import getenv
        remote_host = args.remote_host or getenv("INFINIT_REMOTE_HOST")
        remote_port = args.remote_port or getenv("INFINIT_REMOTE_PORT")
        remote_token_path = args.remote_token_path or getenv("INFINIT_REMOTE_TOKEN_PATH")

        if not remote_host:
            raise Exception("You neither provided --remote-host nor exported INFINIT_REMOTE_HOST.")
        if not remote_port:
            raise Exception("You neither provided --remote-port nor exported INFINIT_REMOTE_PORT.")
        if not remote_token_path:
            raise Exception("You neither provided --remote-token-path nor exported INFINIT_REMOTE_TOKEN_PATH.")

        print_remote(id_ = args.remote_network_identifier,
                     host = remote_host,
                     port = int(remote_port),
                     token_path = remote_token_path,
                     attributes = attributes,
                     format_ = args.format)
    else:
        raise Exception("Neither --local-network-path, --local-descriptor-path nor --remote-network-identifier given")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--remote-network-identifier",
                        help = "XXX: The identifier of the network.")
    parser.add_argument("--remote-host",
                        help = "XXX: The host. You can also export INFINIT_REMOTE_HOST.")
    parser.add_argument("--remote-port",
                        type = int,
                        help = "XXX: The port. You can also export INFINIT_REMOTE_PORT.")
    parser.add_argument("--remote-token-path",
                        help = "XXX: The token path. You can also export INFINIT_REMOTE_TOKEN_PATH.")

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

    import sys
    try:
        args = parser.parse_args()
        main(args)
        sys.exit(0)
    except Exception as e:
        print(e)
        sys.exit(1)
