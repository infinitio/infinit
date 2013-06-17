def meta_to_parser(parser):
    parser.add_argument("--meta-host",
                        help = "XXX: The host. You can also export INFINIT_META_HOST.")
    parser.add_argument("--meta-port",
                        type = int,
                        help = "XXX: The port. You can also export INFINIT_META_PORT.")
    parser.add_argument("--meta-token-path",
                        help = "XXX: The token path. You can also export INFINIT_META_TOKEN_PATH.")

def meta_values(args):
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

    return meta_host, int(meta_port), meta_token_path

def format_string(string):
    if not isinstance(string, str) or len(string) == 0:
        return string
    if string[-1:] != ".":
        string = string + "."
    return string[0].upper() + string[1:]

def run(parser, main):
    import sys
    try:
        args = parser.parse_args()
        main(args)
        sys.exit(0)
    except Exception as e:
        print(format_string(str(e)))
        sys.exit(1)
