#! /usr/bin/env python3

import test
import reactor
import time
def go():
  with test.Oracles() as o:
    time.sleep(2)


s = reactor.Scheduler()
t = reactor.Thread(s, "test starter", lambda : go())
s.run()



