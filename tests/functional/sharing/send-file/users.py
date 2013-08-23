#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import gap

import os
import sys
import tempfile
import time

from utils import generator, email_generator

class Transaction:
    def __init__(self, state, id, status):
        self.id = id
        self.state = state
        self.localy_accepted = False
        self.finished = False
        self.status = status

    def status_dict(self, reverse = False):
        if reverse:
            return {
                "NewTransaction": self.state.TransferState.NewTransaction,
                "SenderCreateNetwork": self.state.TransferState.SenderCreateNetwork,
                "SenderCreateTransaction": self.state.TransferState.SenderCreateTransaction,
                "SenderCopyFiles": self.state.TransferState.SenderCopyFiles,
                "SenderWaitForDecision": self.state.TransferState.SenderWaitForDecision,
                "RecipientWaitForDecision": self.state.TransferState.RecipientWaitForDecision,
                "RecipientAccepted": self.state.TransferState.RecipientAccepted,
                "GrantPermissions": self.state.TransferState.GrantPermissions,
                "PublishInterfaces": self.state.TransferState.PublishInterfaces,
                "Connect": self.state.TransferState.Connect,
                "PeerDisconnected": self.state.TransferState.PeerDisconnected,
                "PeerConnectionLost": self.state.TransferState.PeerConnectionLost,
                "Transfer": self.state.TransferState.Transfer,
                "CleanLocal": self.state.TransferState.CleanLocal,
                "CleanRemote": self.state.TransferState.CleanRemote,
                "Finished": self.state.TransferState.Finished,
                "Rejected": self.state.TransferState.Rejected,
                "Canceled": self.state.TransferState.Canceled,
                "Failed": self.state.TransferState.Failed,
                }
        else:
            return {
                self.state.TransferState.NewTransaction: "NewTransaction",
                self.state.TransferState.SenderCreateNetwork: "SenderCreateNetwork",
                self.state.TransferState.SenderCreateTransaction: "SenderCreateTransaction",
                self.state.TransferState.SenderCopyFiles: "SenderCopyFiles",
                self.state.TransferState.SenderWaitForDecision: "SenderWaitForDecision",
                self.state.TransferState.RecipientWaitForDecision: "RecipientWaitForDecision",
                self.state.TransferState.RecipientAccepted: "RecipientAccepted",
                self.state.TransferState.GrantPermissions: "GrantPermissions",
                self.state.TransferState.PublishInterfaces: "PublishInterfaces",
                self.state.TransferState.Connect: "Connect",
                self.state.TransferState.PeerDisconnected: "PeerDisconnected",
                self.state.TransferState.PeerConnectionLost: "PeerConnectionLost",
                self.state.TransferState.Transfer: "Transfer",
                self.state.TransferState.CleanLocal: "CleanLocal",
                self.state.TransferState.CleanRemote: "CleanRemote",
                self.state.TransferState.Finished: "Finished",
                self.state.TransferState.Rejected: "Rejected",
                self.state.TransferState.Canceled: "Canceled",
                self.state.TransferState.Failed: "Failed",
        }

    @property
    def progress(self):
        return self.state.transaction_progress(self.id)

    def __str__(self):
        return "Transaction(%s, %s, localy_accepted: %s)" % (self.id, self.status, self.localy_accepted)

    def __repr__(self):
        return "Transaction(%s, %s, localy_accepted: %s)" % (self.id, self.status, self.localy_accepted)

# Default user. Accept transaction when 'initialized' status is triggered.
# On __init__, register the first time and just login afterward.
class User:
    def __init__(self,
                 meta_server = None,
                 trophonius_server = None,
                 apertus_server = None,
                 fullname = None,
                 register = False,
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
        self.output_dir = output_dir
        self.use_temporary = self.output_dir is None
        self.temporary_output_dir = None
        self.machine_id = 0
        self.transactions = dict()

    def _init_state(self):
        # state setup
        assert self.state == None
        self.state = gap.State(
            "localhost", int(self.meta_server.meta_port),
            "0.0.0.0", int(self.trophonius_server.port),
            "0.0.0.0", int(self.apertus_server.port)
        )
        self.state.__enter__()

        if self.use_temporary:
            assert self.temporary_output_dir is None
            self.temporary_output_dir = tempfile.TemporaryDirectory()
            self.temporary_output_dir.__enter__()
            self.output_dir = self.temporary_output_dir.name
        if not os.path.exists(self.output_dir):
            os.path.makedirs(self.output_dir)
        assert os.path.isdir(self.output_dir)
        self.state.set_output_dir(self.output_dir)

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
                self.state.__exit__(type, value, tb)
        finally:
            self.state = None

        # output directory cleanup
        try:
            if self.use_temporary:
                assert self.temporary_output_dir is not None
                self.temporary_output_dir.__exit__(type, value, tb)
        except Exception as e:
            import sys
            print("Couldn't remove %s output directory: %s" % (self.output_dir, e),
                  file = sys.stderr)
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
        self.machine_id = self.state.send_files_by_email(email, files)

    def _send_to_user(self, files, recipient):
        assert isinstance(recipient, User)
        self.machine_id = self.state.send_files_by_email(recipient.email, files)

    def _on_transaction(self, transaction_id, status):
        if transaction_id not in self.transactions:
            self.transactions[transaction_id] = Transaction(
                self.state, transaction_id, status)
        else:
            self.transactions[transaction_id].status = status

        self.on_transaction(transaction_id, status)

    def on_transaction(self, transaction_id, status):
        state = self.state
        print("{} Transaction {} {}".format(state._id, transaction_id, status))
        assert self.id == state.transaction_recipient_id(transaction_id) or \
               self.id == state.transaction_sender_id(transaction_id)
        is_sender = (self.id == self.state.transaction_sender_id(transaction_id))
        if status == state.TransactionStatus.Canceled:
            print("Transaction canceled")
            state.join_transaction(transaction_id)
        elif status == state.TransactionStatus.Failed:
            print("Transaction failed")
            state.join_transaction(transaction_id)
        elif not is_sender and status == state.TransactionStatus.RecipientWaitForDecision:
            self.machine_id = state.accept_transaction(transaction_id)
        elif status == state.TransactionStatus.Finished:
            self.transactions[transaction_id].finished = True
            print("{} join {}".format(state._id, status))
            self.state.join_transaction(transaction_id)
            print("{} joined {}".format(state._id, status))

# This user recreate a new email and reregister on __enter__.
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

# This user does nothing.
class DoNothingUser(User):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)

        def on_transaction(self, transaction_id, status, is_new):
            pass

# This user cancel transaction when the specified state is triggered
# after a specified delay.
class CancelUser(User):
    def __init__(self, when = None, delay = 0, *args, **kwargs):
        assert when is not None
        super().__init__(*args, **kwargs)
        self.when = when
        self.delay = delay

    def on_transaction(self, transaction_id, status, is_new):
        assert is_new
        state = self.state
        transaction = self.transactions[transaction_id]
        assert self.id == state.transaction_recipient_id(transaction_id) or \
               self.id == state.transaction_sender_id(transaction_id)
        is_sender = (self.id == self.state.transaction_sender_id(transaction_id))
        if status == state.TransactionStatus.Canceled:
            state.join_transaction(transaction_id)
        elif status == state.TransactionStatus.Failed:
            print("Transaction failed")
            state.join_transaction(transaction_id)
        if (status == transaction.status_dict(reverse = True)[self.when]):
            time.sleep(self.delay / 1000)
            state.cancel_transaction(transaction_id)
            state.join_transaction(transaction_id)
