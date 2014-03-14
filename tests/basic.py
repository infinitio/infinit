import test
import reactor
import time
def go():
  with test.Oracles() as o:
    time.sleep(2)


s = reactor.Scheduler()
t = reactor.Thread(s, "coucou", lambda : go())
s.run()



