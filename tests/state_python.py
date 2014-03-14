#!/usr/bin/env python3

import reactor
import state
import unittest

class Wrapper:

  def __init__(self, email='chris2@infinit.io', password='password'):
    self.email = email
    self.password = password

  def _login(self):
    test_state = state.State('https', 'development.infinit.io', 443,
                             'development.infinit.io', 444)
    hashed_password = state.hash_password(self.email, self.password)
    test_state.login(self.email, hashed_password)
    return test_state

  def login_logout_test(self, email='chris2@infinit.io', password='password'):
    test_state = self._login()
    test_state.logout()

  def user_list_test(self):
    test_state = self._login()
    users = test_state.users()
    print(users)

  def transaction_list_test(self):
    test_state = self._login()
    transactions = test_state.transactions()
    print(transactions)

class TestState(unittest.TestCase):

  def test_login_logout(self):
    wrapper = Wrapper()
    sched = reactor.Scheduler()
    thread = reactor.Thread(sched, 'state', lambda: wrapper.login_logout_test())
    sched.run()

  def test_user_list(self):
    wrapper = Wrapper()
    sched = reactor.Scheduler()
    thread = reactor.Thread(sched, 'state', lambda: wrapper.user_list_test())
    sched.run()

  def test_transaction_list(self):
    wrapper = Wrapper()
    sched = reactor.Scheduler()
    thread = reactor.Thread(sched, 'state',
                            lambda: wrapper.transaction_list_test())
    sched.run()

unittest.main()
