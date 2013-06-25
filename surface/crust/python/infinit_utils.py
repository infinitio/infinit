import argparse
class LocalParser(argparse.ArgumentParser):
    def __init__(self, require_infinit_home = True):
        super().__init__()
        self.require_infinit_home = require_infinit_home
        self.add_argument("--infinit-home",
                          help = "XXX: The path to your infinit home directory")
        self.add_argument("--user-name",
                          help = "XXX: WILL BE REMOVE, IT'S YOU, IT'S YOUR NAME")
    def parse_args(self):
        args = super().parse_args()
        from os import path, getenv
        home = args.infinit_home or getenv("INFINIT_HOME")
        if not home:
            raise Exception("You must provide a home directory, with --infinit-home or env INFINIT_HOME")
        if self.require_infinit_home and not path.exists(home):
            raise Exception("Provided path %s via --infinit-home doesn't exist" % home)
        if not args.user_name:
            raise Exception("You must provide a --user-name, it's tempory.")

        setattr(args, "infinit_home", home)
        return args

class AuthorityParser(argparse.ArgumentParser):
    def __init__(self):
        super().__init__()

        self.add_argument("--authority-host",
                          help = "XXX: The identity host")
        self.add_argument("--authority-port",
                          help = "XXX: The identity port")
    def parse_args(self):
        args = super().parse_args()
        from os import getenv

        authority_host = args.authority_host or getenv("INFINIT_AUTHORITY_HOST")
        if authority_host is None:
            raise Exception("You neither provided --authority-host nor exported INFINIT_AUTHORITY_HOST.")
        setattr(args, "authority_host", authority_host)

        authority_port = args.authority_port or getenv("INFINIT_AUTHORITY_PORT")
        if authority_port is None:
            raise Exception("You neither provided --authority-port nor exported INFINIT_AUTHORITY_PORT.")
        setattr(args, "authority_port", int(authority_port))

        return args

class RemoteParser(argparse.ArgumentParser):
    def __init__(self, require_token = True):
        super().__init__()
        self.require_token = require_token
        self.add_argument("--meta-host",
                          help = "XXX: The host. You can also export INFINIT_META_HOST.")
        self.add_argument("--meta-port",
                          type = int,
                          help = "XXX: The port. You can also export INFINIT_META_PORT.")

        if self.require_token:
            self.add_argument("--meta-token-path",
                              help = "XXX: The token path. You can also export INFINIT_META_TOKEN_PATH.")

    def parse_args(self):
        args = super().parse_args()
        from os import getenv, path

        meta_host = args.meta_host or getenv("INFINIT_META_HOST")
        if meta_host is None:
            raise Exception("You neither provided --meta-host nor exported INFINIT_META_HOST.")
        setattr(args, "meta_host", meta_host)

        meta_port = args.meta_port or getenv("INFINIT_META_PORT")
        if meta_port is None:
            raise Exception("You neither provided --meta-port nor exported INFINIT_META_PORT.")
        setattr(args, "meta_port", int(meta_port))

        if self.require_token:
            meta_token_path = args.meta_token_path or getenv("INFINIT_META_TOKEN_PATH")
            if meta_token_path is None:
                raise Exception("You neither provided --meta-token-path nor exported INFINIT_META_TOKEN_PATH.")
            if not path.exists(meta_token_path):
                raise Exception("meta-token-path at %s doesn't exist" % meta_token_path)
            setattr(args, "meta_token_path", meta_token_path)

        return args

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
