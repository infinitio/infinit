#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import os
from users import User
from scenarii import DefaultScenario
import utils

if __name__ == '__main__':

    import utils
    with utils.Servers() as (meta, trophonius, apertus):
        # Forwarder.
        os.environ["INFINIT_LOCAL_ADDRESS"] = "128.128.83.31"
        sender, recipient = (User(meta_server = meta,
                                  trophonius_server = trophonius,
                                  apertus_server = apertus,
                                  register = True),
                             User(meta_server = meta,
                                  trophonius_server = trophonius,
                                  apertus_server = apertus,
                                  register = True))

        for item in utils.cases():
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                DefaultScenario(sender = sender,
                                recipient = recipient,
                                files = files).run()
