#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import gap
import random
import hashlib

import os

import string
def generator(size = 12, prefix = "", chars = string.ascii_lowercase):
    return prefix + ''.join(random.choice(chars) for x in range(size))

def email_generator(size = 12, prefix = "", chars = string.ascii_lowercase):
    return generator(size, prefix, chars) + "@infinit.io"

class TestFailure(Exception):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

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
