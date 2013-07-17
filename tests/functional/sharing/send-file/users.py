#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import gap

import os
import sys
import tempfile
from utility import generator, email_generator

class Transaction:
    def __init__(self, state, id):
        assert isinstance(id, str)
        self.id = id
        self.state = state
        self.localy_accepted = False
        self.finished = False

    @property
    def status(self):
        return {
            self.state.TransactionStatus.created: "created",
            self.state.TransactionStatus.started: "started",
            self.state.TransactionStatus.canceled: "canceled",
            self.state.TransactionStatus.failed: "failed",
            self.state.TransactionStatus.finished: "finished",
        }[self.state.transaction_status(self.id)]

    @property
    def progress(self):
        return self.state.transaction_progress(self.id)

    # @property
    # def status(self):
    #     return self.state.transaction_status(self.id)

    def __str__(self):
        return "Transaction(%s, %s, localy_accepted: %s)" % (self.id, self.status, self.localy_accepted)

    def __repr__(self):
        return "Transaction(%s, %s, localy_accepted: %s)" % (self.id, self.status, self.localy_accepted)

class User:
    def __init__(self,
                 meta_server = None,
                 trophonius_server = None,
                 apertus_server = None,
                 fullname = None,
                 register = False,
                 early_accept = False,
                 output_dir = None):
        assert meta_server is not None
        assert trophonius_server is not None
        assert apertus_server is not None
        self.meta_server = meta_server
        self.trophonius_server = trophonius_server
        self.apertus_server = apertus_server
        self.state = None
        self.fullname = fullname is None and generator(6) or fullname
        self.email = self.fullname + "@infinit.io"
        self.register = register
        self.early_accept = early_accept
        self.output_dir = output_dir
        self.use_temporary = self.output_dir is None
        self.temporary_output_dir = None

    def _init_state(self):
        # state setup
        assert self.state == None
        self.state = gap.State(
            "localhost", int(self.meta_server.meta_port),
            "0.0.0.0", int(self.trophonius_server.port),
            "0.0.0.0", int(self.apertus_server.port)
        )
        self.state.__enter__()

    def _register_or_login(self):
        if self.register:
            assert self.email is not None
            assert self.fullname is not None
            self.state.register(
                self.fullname,
                self.email,
                "password",
                self.fullname + "_device",
                "bitebite"
            )
            self.register = False
        else:
            self.state.login(self.email, "password")

        # Setting output dir requiere the TransactionManager, which requiere
        # to be logged in...
        if self.use_temporary:
            assert self.temporary_output_dir is None
            self.temporary_output_dir = tempfile.TemporaryDirectory()
            self.temporary_output_dir.__enter__()
            self.output_dir = self.temporary_output_dir.name
        if not os.path.exists(self.output_dir):
            os.path.makedirs(self.output_dir)
        assert os.path.isdir(self.output_dir)
        self.state.set_output_dir(self.output_dir)
        self.state.transaction_callback(self._on_transaction)

        self.id = self.state._id
        self.transactions = dict()

    def __enter__(self):
        self._init_state()
        self._register_or_login()
        return self

    def __exit__(self, type, value, tb):
        # state cleanup
        try:
            if self.state is not None:
                self.state.logout()
                self.state.__exit__(type, value, traceback)
        finally:
            self.state = None

        # output directory cleanup
        try:
            if self.use_temporary:
                assert self.temporary_output_dir is not None
                self.temporary_output_dir.__exit__(type, value, traceback)
        except:
            import sys
            print(
                "Couldn't remove", self.output_dir, "output directory",
                file = sys.stderr
            )
        finally:
            self.temporary_output_dir = None

    def send_files(self, recipient = None, files = None, email = None):
        assert (email is None) != (recipient is None) # xor email or recipient.
        assert isinstance(files, list)
        if email is not None:
            self._send_via_email(files, email)
        else:
            self._send_to_user(files, recipient)

    def _send_via_email(self, files, email):
        # import lepl.apps.rfc3696
        # validator = lepl.apps.rfc3696.Email()
        # assert validator(email)
        self.state.send_files(email, files)

    def _send_to_user(self, files, recipient):
        assert isinstance(recipient, User)
        self.state.send_files(recipient.id, files)

    def _on_transaction(self, transaction_id, status, is_new):
        if transaction_id not in self.transactions:
            self.transactions[transaction_id] = Transaction(
                self.state,
                transaction_id
            )
        self.on_transaction(transaction_id, status, is_new)

    def on_transaction(self, transaction_id, status, is_new):
        assert is_new
        state = self.state
        assert self.id == state.transaction_recipient_id(transaction_id) or \
               self.id == state.transaction_sender_id(transaction_id)
        is_sender = (self.id == self.state.transaction_sender_id(transaction_id))
        if status == state.TransactionStatus.canceled:
            print("Transaction canceled")
            sys.exit(1)
        elif status == state.TransactionStatus.failed:
            print("Transaction canceled")
            sys.exit(1)
        elif not is_sender and not self.transactions[transaction_id].localy_accepted:
            if self.early_accept and status == state.TransactionStatus.created or \
               not self.early_accept and status == state.TransactionStatus.started:
                self.transactions[transaction_id].localy_accepted = True
                state.accept_transaction(transaction_id)
        elif status == state.TransactionStatus.finished:
            self.transactions[transaction_id].finished = True

class GhostUser(User):

    def __init__(self,
                 meta_server = None,
                 trophonius_server = None,
                 apertus_server = None,
                 fullname = None,
                 output_dir = None):
        super().__init__(meta_server = meta_server,
                         trophonius_server = trophonius_server,
                         apertus_server = apertus_server,
                         fullname = fullname,
                         output_dir = output_dir,
                         early_accept = False,
                         register = True)

    def __enter__(self):
        # Turn the user to a new ghost.
        self.email = email_generator(prefix = "recipient", size = 5)
        self._init_state()
        return self

    def __exit__(self, type, value, traceback):
        self.email = None
        self.register = True
        super().__exit__(type, value, traceback)
