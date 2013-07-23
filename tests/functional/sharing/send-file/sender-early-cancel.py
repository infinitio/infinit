#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import os
from users import DoNothingUser, CancelUser
from scenarii import CancelScenario
import utils

if __name__ == '__main__':

    import utils
    with utils.Servers() as (meta, trophonius, apertus):
        # Created cancel.
        sender, recipient = (CancelUser(meta_server = meta,
                                        trophonius_server = trophonius,
                                        apertus_server = apertus,
                                        register = True,
                                        when = "created",
                                        delay = 1000), # ms
                             DoNothingUser(meta_server = meta,
                                           trophonius_server = trophonius,
                                           apertus_server = apertus,
                                           register = True))

        for item in utils.cases():
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                CancelScenario(sender = sender,
                               recipient = recipient,
                               files = files).run()
