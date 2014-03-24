def run(f):
  import reactor
  s = reactor.Scheduler()
  t = reactor.Thread(s, 'main', f)
  s.run()


def expect_raise(f, type = BaseException):
  try:
    f()
  except type:
    pass
  else:
    raise Exception('expected a %s' % type)
