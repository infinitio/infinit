#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import os
from users import User, CancelUser
from scenarii import CancelScenario
import utils

if __name__ == '__main__':
    import utils
    files = [utils.RandomTempFile(100)]
    with utils.Servers() as (meta, trophonius, apertus):
        for status in ["Connect"]:
            for delay in [100, 1000]:
                sender, recipient = (CancelUser(meta_server = meta,
                                                trophonius_server = trophonius,
                                                apertus_server = apertus,
                                                register = True,
                                                when = status,
                                                delay = delay), # ms
                                     User(meta_server = meta,
                                          trophonius_server = trophonius,
                                          apertus_server = apertus,
                                          register = True))
                with sender, recipient:
                    CancelScenario(sender = sender,
                                   recipient = recipient,
                                   files = [file.name for file in files]).run()
