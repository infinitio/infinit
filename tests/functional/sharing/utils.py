#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

# N.B: While we are using sym link, it's hard to keep a track of the other
# files. Each section could be splitted when creating a clean utils library.

import sys
import os

#------------------------------------------------------------------------------
# Email generation
#------------------------------------------------------------------------------
import string
import random

def generator(size = 12, prefix = "", chars = string.ascii_lowercase):
    return prefix + ''.join(random.choice(chars) for x in range(size))

def email_generator(size = 12, prefix = "", chars = string.ascii_lowercase):
    return generator(size, prefix, chars) + "@infinit.io"

#------------------------------------------------------------------------------
# Servers
#------------------------------------------------------------------------------
import meta
import trophonius
import apertus

class Servers:

    def __init__(self):
        self.meta = None
        self.tropho = None

    def __enter__(self):
        port = 39075 # XXX
        self.apertus = apertus.Apertus()
        self.apertus.__enter__()
        self.meta = meta.Meta(
            spawn_db = True,
            trophonius_control_port = port)
        self.meta.__enter__()
        self.tropho = trophonius.Trophonius(
            meta_port = self.meta.meta_port,
            control_port = port)
        self.tropho.__enter__()
        return self.meta, self.tropho, self.apertus

    def __exit__(self, exception_type, exception, *args):
        self.tropho.__exit__(exception_type, exception, *args)
        self.meta.__exit__(exception_type, exception, *args)
        self.apertus.__exit__(exception_type, exception, *args)
        if exception is not None:
            print('======== Trophonius stdout:\n' + self.tropho.stdout,
                  file = sys.stderr)
            print('======== Trophonius stderr:\n' + self.tropho.stderr,
                  file = sys.stderr)
            print('======== Meta stdout:\n' + self.meta.stdout,
                  file = sys.stderr)
            print('======== Meta stderr:\n' + self.meta.stderr,
                  file = sys.stderr)

#------------------------------------------------------------------------------
# Exceptions
#------------------------------------------------------------------------------
class TestFailure(Exception):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

#------------------------------------------------------------------------------
# File/Directory comparison
#------------------------------------------------------------------------------
import hashlib
def _file_sha1(file):
    sha1 = hashlib.sha1()
    while True:
        data = file.read(4096)
        if not data:
            break
        sha1.update(data)
    return sha1.hexdigest()

def dir_sha1(dst, src):
    assert os.path.isdir(dst)
    assert os.path.isdir(src)

    src_dir, src_dirs, src_files = list(os.walk(src))[0]
    dst_dir, dst_dirs, dst_files = list(os.walk(dst))[0]
    src_files.sort()
    dst_files.sort()
    for dst_file, src_file in zip(dst_files, src_files):
        dst_file = os.path.join(dst_dir, dst_file)
        src_file = os.path.join(src_dir, src_file)
        if not os.path.exists(dst_file):
            raise TestFailure("{} does not exists".format(dst_file))
        if not os.path.exists(src_file):
            raise TestFailure("{} does not exists".format(src_file))
        if file_sha1(dst_file) != file_sha1(src_file):
            raise TestFailure("sha1({}) != sha1({})".format(dst_file, src_file))

def file_sha1(file):
    if isinstance(file, str):
        with open(file, 'rb') as handle:
            return _file_sha1(handle)
    else:
        old_pos = file.tell()
        try:
            return _file_sha1(file)
        finally:
            file.seek(old_pos)

#------------------------------------------------------------------------------
# File/Directory generation
#------------------------------------------------------------------------------
import tempfile

class RandomTempFile:
    def __init__(self,
                 size = 0,
                 random_ratio : "How many bytes are randomized" = 0.1,
                 **kw):
        self.size = size
        self.random_ratio = random_ratio
        self.file = tempfile.NamedTemporaryFile(**kw)
        if self.size > 0:
            self.file.truncate(self.size)
        if self.random_ratio:
            to_randomize = self.size * self.random_ratio
            randomized = 0
            while randomized < to_randomize:
                pos = random.randint(0, self.size - 1)
                self.file.seek(pos)
                to_write = min(self.size - 1 - pos, random.randint(0, 4096))
                if to_write <= 0:
                    continue
                data = bytes(chr(random.randint(1, 255)), 'utf8') * to_write
                self.file.write(data[:to_write])
                randomized += to_write
        self.file.seek(0)
        assert os.path.getsize(self.file.name) == self.size
        self._sha1 = None

    def __enter__(self):
        self.file.__enter__()
        return self

    def __exit__(self, type, value, traceback):
        self.file.__exit__(type, value, traceback)

    def __getattr__(self, attr):
        return getattr(self.file, attr)

    @property
    def sha1(self):
        if self._sha1 is None:
            self._sha1 = file_sha1(self.file)
        return self._sha1

class RandomDirectory(tempfile.TemporaryDirectory):
    def __init__(self, file_count = 15, min_file_size = 1024 * 1024, max_file_size = 1024 * 1024 * 10):
        if min_file_size > max_file_size: min_file_size, max_file_size = max_file_size, min_file_size
        tempfile.TemporaryDirectory.__init__(self, prefix="tmpdir-")
        self.files = [RandomTempFile(random.randint(min_file_size, max_file_size), dir=self.name) for x in range(file_count)]

    def __enter__(self):
        super().__enter__()
        for f in self.files:
            f.__enter__()
        return self

    def __exit__(self, exception_type, exception, bt):
        for f in self.files:
            f.__exit__(exception_type, exception, bt)
        super().__exit__(exception_type, exception, bt)

    def __del__(self):
        for f in self.files:
            f.__del__()
        super().__del__()

#------------------------------------------------------------------------------
# Cases.
#------------------------------------------------------------------------------
def cases():
    return [
        RandomTempFile(100), # Mono file.
        [RandomTempFile(40)] * 2, # Multi files.
        RandomDirectory(file_count = 50, min_file_size = 10, max_file_size = 50), # Mono Folder.
        [RandomDirectory(file_count = 3, min_file_size = 10, max_file_size = 50)] * 2, # Many Folders.
    ]
