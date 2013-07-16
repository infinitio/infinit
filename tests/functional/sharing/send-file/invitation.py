#!/usr/bin/env python3
# -*- encoding: utf-8 --*

import os
from filesystem import RandomDirectory, RandomTempFile
from users import GhostUser, User
from scénarii import GhostScenario
import utils

if __name__ == '__main__':

    cases = [
        [RandomTempFile(4)] * 10,
        RandomTempFile(400),
        RandomDirectory(file_count = 512, min_file_size = 10, max_file_size = 1024),
        [RandomDirectory(file_count = 10, min_file_size = 128, max_file_size = 2048), RandomTempFile(100), RandomTempFile(40000)],
    ]

    import utils
    with utils.Servers() as (meta, trophonius, apertus):
        # Invitations.
        sender, recipient = (User(meta_server = meta,
                                  trophonius_server = trophonius,
                                  apertus_server = apertus,
                                  register = True),
                             GhostUser(meta_server = meta,
                                       trophonius_server = trophonius,
                                       apertus_server = apertus))

        for item in cases[0:3]:
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                GhostScenario(sender = sender,
                              recipient = recipient,
                              files = files).run(timeout = 1024 * 1024)
