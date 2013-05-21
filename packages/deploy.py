#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

"""Deployment script.
"""

import argparse
import os
import subprocess
import sys
import libpkg

parser = argparse.ArgumentParser(description="Deploy infinit packages")
parser.add_argument(
    "--match", '-m',
    help="filter with a simple pattern",
    action="append"
)
parser.add_argument(
    '--last', '-l',
    help="Install the last tarball in the results",
    action='store_true',
    default=False
)

parser.add_argument(
    '--download-dir', '-D',
    help="Keep (and use) downloaded builds in this directory",
    action="store",
    type=str,
    default=None,
)

parser.add_argument(
    '--print',
    help="Only print matching releases",
    action='store_true',
    default=False
)
parser.add_argument(
    '--yes', '-y',
    help="Assume yes to all question",
    action='store_true',
    default=False,
)
parser.add_argument(
    '--local',
    help="Use local builds",
    action='store_true',
    default=False
)
parser.add_argument(
    '--only-client', '-c',
    help="Use only client builds",
    action='store_true',
    default=False
)
parser.add_argument(
    '--only-server', '-s',
    help="Use only server builds",
    action='store_true',
    default=False
)
parser.add_argument(
    '--dest-dir', '-d',
    help="Set destination directory for built packages",
    action='store',
    default='.'
)
parser.add_argument(
    '--package', '-p',
    help="Add a package type (in %s)" % ', '.join(p(None,None).name.lower() for p in libpkg.PACKAGERS),
    action='append',
)
parser.add_argument(
    '--all-packages', '-a',
    help="Add all package types",
    action='store_true',
    default=False
)


def dumpReleases(releases):
    count = 0
    for hash, tarballs in releases.items():
        count += 1
        #if count > 30:
        #    print("Ignoring", len(releases) - count, "releases")
        #    break
        date = None
        client = False
        server = False

        for tarball, tarball_date in tarballs:
            tarball_date = tarball_date.strip()
            if date is not None:
                print(date, tarball_date)
            date = tarball_date.strip()
            if 'client' in tarball:
                assert client == False
                client = True
            elif 'server' in tarball:
                assert server == False
                server = True
            print('- %s (%s): ' % (hash, date),
                  ' / '.join(s for s in [
                      client and 'client' or '',
                      server and 'server' or ''
                  ] if s))



def yesno(s, default=False):
    res = None
    if default:
        s += ' (Y/n): '
    else:
        s += ' (y/N): '
    while res is None:
        v = input(s).strip()
        if not v: return default
        elif v in ['y', 'Y', 'o', 'O']:
            return True
        elif v in ['n', 'N']:
            return False


def deployClientTarball(package):
    os.system('scp "%s" oracle@development.infinit.io:www/development.infinit.io/downloads' % package.path)

    dir_ = {
        libpkg.constants.Platforms.LINUX: 'linux',
        libpkg.constants.Platforms.MACOSX: 'macosx',
        libpkg.constants.Platforms.WINDOWS: 'win',
    }[package.platform] + {
        libpkg.constants.Architectures.I386: '32',
        libpkg.constants.Architectures.AMD64: '64',
    }[package.architecture]

    cmd = ' && '.join([
        "cd www/development.infinit.io/downloads",
        "mkdir -p %(dir)s",
        "tar --extract --file=%(tarball)s --strip-components=1 --directory=%(dir)s",
        #"rm %(tarball)s",
        "chmod -R a+rX %(dir)s",
    ]) % {
        'dir': dir_,
        'tarball': package.file_,
    }
    os.system('ssh oracle@development.infinit.io "%s"' % cmd)

def deployServerTarball(package):
    os.system('scp  %s oracle@development.infinit.io:www/development.infinit.io/' % package.path)
    cmd = ' && '.join([
        "cd www/development.infinit.io",
        "tar --extract --file=%(tarball)s --strip-components=1 --directory=.",
        "rm %(tarball)s",
    ]) % {
        'tarball': package.file_,
    }
    os.system('ssh oracle@development.infinit.io "%s"' % cmd)
    if yesno("Restart api server ?", True):
        cmd = 'sudo /etc/init.d/meta restart && sleep 3'
        subprocess.call('ssh -t development.infinit.io "%s"' % cmd, shell=True)

def deployTarball(package):
    assert package.type_ in ('client', 'server')
    if package.type_ == 'server':
        deployServerTarball(package)
    else:
        deployClientTarball(package)

def deployPackage(package):
    if package.kind == 'Archive':
        deployTarball(package)
    else:
        os.system('scp "%s" oracle@development.infinit.io:www/infinit.im/downloads' % package.path)

def getFarmBuild(infos, args):
    if args.last:
        tarballs = libpkg.farm.getLastTarballs(args.match)
    else:
        tarballs = libpkg.farm.getTarballs(args.match)

    releases = {}
    for tarball, date in tarballs:
        h = libpkg.farm.getTarballHash(tarball)
        releases.setdefault(h, [])
        releases[h].append((tarball, date))


    to_install = None
    if not len(releases):
        print("No release found")
    elif len(releases) == 1:
        to_install = list(releases.keys())[0]
    else:
        print("More than one release match:")
        dumpReleases(releases)
        print("Use --match with a pattern or --last")

    if not to_install:
        sys.exit(not args.print and 1 or 0)

    infos['download_dir'] = args.download_dir
    return libpkg.FarmBuild(infos, to_install, releases[to_install])

def preparePackages(args, build, packagers, build_client, build_server):
    if not os.path.exists(args.dest_dir):
        os.mkdir(args.dest_dir)
    assert os.path.isdir(args.dest_dir)
    packages = []
    for packager in packagers:
        packages.extend(
            packager.buildPackages(build, args.dest_dir, build_client, build_server)
        )

    return packages

if __name__ == '__main__':
    args = parser.parse_args()

    infos = {
        'version': '0.4',
        'version_name': 'alpha',
        'server_architecture': libpkg.constants.Architectures.AMD64,
        'server_platform': libpkg.constants.Platforms.LINUX,
    }

    if args.local:
        build = libpkg.LocalBuild(infos, os.path.join(os.path.dirname(__file__), '../build'))
    else:
        build = getFarmBuild(infos, args)

    build_client = build.has_client and not args.only_server
    build_server = build.has_server and not args.only_client

    print("Selected build (", build.hash, "):")
    print("\t- Server build:", build_server, 'at', build.client_date)
    print("\t- Client build:", build_client, 'at', build.server_date)
    print("\t- Architecture(s):", ', '.join(build.architectures_strings))
    print("\t- Platform(s):", ', '.join(build.platforms_strings))
    print("\t- Status:", build.is_available and "Working" or "Not working")


    print()

    packagers = []
    selected_packages = list(s.lower() for s in (args.package or []))
    print("Packagers found:")
    for packager_cls in libpkg.PACKAGERS:
        packager = packager_cls(build.architectures, build.platforms)
        used = False
        if packager.is_available:
            if args.all_packages or packager.name.lower() in selected_packages:
                packagers.append(packager)
                used = True
        print("\t-", packager.name,':', packager.status, (not used and "(Not used)" or "(Selected)"))

    print()
    print(len(packagers), "packager(s) selected.")

    if args.print:
        sys.exit(0)

    if not packagers:
        if not args.all_packages:
            print("Use --all-packages or specify one or more package type with --package")
        else:
            print("No packager available")
        sys.exit(1)

    if not (args.yes or yesno("Proceed ?", True)):
        sys.exit(1)

    with build:
        packages = preparePackages(args, build, packagers, build_client, build_server)

    if not packages:
        print("No packages built.")
        sys.exit(0)

    print()
    print("Built packages:")
    for package in packages:
        print('\t-', package)


    if not (args.yes or yesno("Deploy these packages ?", True)):
        sys.exit(1)

    for package in packages:
        print("Deploying `%s':" % package.path)
        deployPackage(package)
