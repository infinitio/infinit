#! /usr/bin/env python3

import datetime
import infinit.oracles.servers
import reactor
import time

def main():
  with infinit.oracles.servers.Oracles() as o:
    reactor.sleep(datetime.timedelta(seconds = 2))

s = reactor.Scheduler()
t = reactor.Thread(s, 'main', main)
s.run()
