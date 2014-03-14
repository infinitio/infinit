import test
import reactor
def go():
  (servers, addr_meta, addr_trophonius, addr_apertus) = test.oracles()


s = reactor.Scheduler()
t = reactor.Thread(s, "coucou", lambda : go())
s.run()



