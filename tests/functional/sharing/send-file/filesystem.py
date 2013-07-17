#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import os
import tempfile
import random

from utility import file_sha1

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
