#!/usr/bin/env python3

import reactor
import state
import unittest

class Wrapper:

  def __init__(self, email='chris2@infinit.io', password='password'):
    self.email = email
    self.password = password

  def _transaction_callback(self, notification):
    print('transaction_id=%s status=%s' % (notification['transaction_id'], notification['status']))

  def _initialise_state(self,
                        transaction_callback=False):
    test_state = state.State('https', 'development.infinit.io', 443,
                             'development.infinit.io', 444)
    if transaction_callback:
      test_state.attach_transaction_callback(self._transaction_callback)
    return test_state

  def _login(self, test_state):
    test_state.login(self.email, self.password)

  def login_logout_test(self, email='chris2@infinit.io', password='password'):
    test_state = self._initialise_state()
    self._login(test_state)
    test_state.logout()

  def user_list_test(self):
    test_state = self._initialise_state()
    self._login(test_state)
    users = test_state.users()
    print(users)

  def transaction_list_test(self):
    test_state = self._initialise_state()
    self._login(test_state)
    transactions = test_state.transactions()
    print(transactions)

  def callback_test(self):
    test_state = self._initialise_state(transaction_callback=True)

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

  def test_callbacks(self):
    wrapper = Wrapper()
    sched = reactor.Scheduler()
    thread = reactor.Thread(sched, 'state', lambda: wrapper.callback_test())
    sched.run()

unittest.main()
