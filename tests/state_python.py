#!/usr/bin/env python3

import reactor
import state
import unittest

class Wrapper:

  def login_logout_test(self, email='chris@infinit.io', password='password'):
    test_state = state.State('https', 'development.infinit.io', 443,
                             'development.infinit.io', 444)
    hashed_password = state.State.hash_password(email, password)
    test_state.login(email, hashed_password)
    test_state.logout()

class TestState(unittest.TestCase):

  def test_login(self):
    wrapper = Wrapper()
    sched = reactor.Scheduler()
    thread = reactor.Thread(sched, 'state', lambda: wrapper.login_logout_test())
    sched.run()

unittest.main()
