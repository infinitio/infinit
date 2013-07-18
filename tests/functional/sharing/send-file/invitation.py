#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import os
from filesystem import RandomDirectory, RandomTempFile
from users import GhostUser, User
from scenarii import GhostScenario
import utils

if __name__ == '__main__':

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

        for item in utils.cases()[0:3]:
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                GhostScenario(sender = sender,
                              recipient = recipient,
                              files = files).run()
