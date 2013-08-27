#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import gap

import os
import sys
import tempfile
import time

from utils import generator, email_generator, cprint, Color

class Transaction:
    def __init__(self, state, id, status):
        self.id = id
        self.state = state
        self.localy_accepted = False
        self.finished = False
        self.canceled = False
        self.failed = False
        self.rejected = False
        self.status = status

    def status_dict(self, reverse = False):
        if reverse:
            return {
                "NewTransaction": self.state.TransactionStatus.NewTransaction,
                "SenderCreateNetwork": self.state.TransactionStatus.SenderCreateNetwork,
                "SenderCreateTransaction": self.state.TransactionStatus.SenderCreateTransaction,
                "SenderCopyFiles": self.state.TransactionStatus.SenderCopyFiles,
                "SenderWaitForDecision": self.state.TransactionStatus.SenderWaitForDecision,
                "RecipientWaitForDecision": self.state.TransactionStatus.RecipientWaitForDecision,
                "RecipientAccepted": self.state.TransactionStatus.RecipientAccepted,
                "RecipientWaitForReady": self.state.TransactionStatus.RecipientWaitForReady,
                "GrantPermissions": self.state.TransactionStatus.GrantPermissions,
                "PublishInterfaces": self.state.TransactionStatus.PublishInterfaces,
                "Connect": self.state.TransactionStatus.Connect,
                "PeerDisconnected": self.state.TransactionStatus.PeerDisconnected,
                "PeerConnectionLost": self.state.TransactionStatus.PeerConnectionLost,
                "Transfer": self.state.TransactionStatus.Transfer,
                "CleanLocal": self.state.TransactionStatus.CleanLocal,
                "CleanRemote": self.state.TransactionStatus.CleanRemote,
                "Finished": self.state.TransactionStatus.Finished,
                "Rejected": self.state.TransactionStatus.Rejected,
                "Canceled": self.state.TransactionStatus.Canceled,
                "Failed": self.state.TransactionStatus.Failed,
                }
        else:
            return {
                self.state.TransactionStatus.NewTransaction: "NewTransaction",
                self.state.TransactionStatus.SenderCreateNetwork: "SenderCreateNetwork",
                self.state.TransactionStatus.SenderCreateTransaction: "SenderCreateTransaction",
                self.state.TransactionStatus.SenderCopyFiles: "SenderCopyFiles",
                self.state.TransactionStatus.SenderWaitForDecision: "SenderWaitForDecision",
                self.state.TransactionStatus.RecipientWaitForDecision: "RecipientWaitForDecision",
                self.state.TransactionStatus.RecipientAccepted: "RecipientAccepted",
                self.state.TransactionStatus.RecipientWaitForReady: "RecipientWaitForReady",
                self.state.TransactionStatus.GrantPermissions: "GrantPermissions",
                self.state.TransactionStatus.PublishInterfaces: "PublishInterfaces",
                self.state.TransactionStatus.Connect: "Connect",
                self.state.TransactionStatus.PeerDisconnected: "PeerDisconnected",
                self.state.TransactionStatus.PeerConnectionLost: "PeerConnectionLost",
                self.state.TransactionStatus.Transfer: "Transfer",
                self.state.TransactionStatus.CleanLocal: "CleanLocal",
                self.state.TransactionStatus.CleanRemote: "CleanRemote",
                self.state.TransactionStatus.Finished: "Finished",
                self.state.TransactionStatus.Rejected: "Rejected",
                self.state.TransactionStatus.Canceled: "Canceled",
                self.state.TransactionStatus.Failed: "Failed",
        }
    @property
    def finished_status(self):
        return {
            "finished": self.finished,
            "canceled": self.canceled,
            "rejected":  self.rejected,
            "failed" : self.failed,
            }

    @property
    def final(self):
        return sum(status for status in list(self.finished_status.values())) > 0

    @property
    def progress(self):
        return self.state.transaction_progress(self.id)

    def __str__(self):
        return "Transaction(%s, %s, %s)" % (self.id, self.status, self.finished_status)

    def __repr__(self):
        return "Transaction(%s, %s, %s)" % (self.id, self.status, self.finished_status)
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
        cprint("Transaction %s %s" % (transaction_id, status), color=Color.Green)
        assert self.id == state.transaction_recipient_id(transaction_id) or \
               self.id == state.transaction_sender_id(transaction_id)
        transaction = self.transactions[transaction_id]
        is_sender = (self.id == self.state.transaction_sender_id(transaction_id))
        if status == state.TransactionStatus.Canceled:
            print("%s Transaction canceled" % status)
            transaction.canceled = True
            state.join_transaction(transaction_id)
        elif status == state.TransactionStatus.Failed:
            print("%s Transaction failed" % status)
            transaction.failed = True
            state.join_transaction(transaction_id)
        elif status == state.TransactionStatus.Rejected:
            print("%s Transaction rejected" % status)
            transaction.rejected = True
            state.join_transaction(transaction_id)
        elif not is_sender and status == state.TransactionStatus.RecipientWaitForDecision:
            self.machine_id = state.accept_transaction(transaction_id)
        elif status == state.TransactionStatus.Finished:
            transaction.finished = True
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

    def on_transaction(self, transaction_id, status):
        state = self.state
        transaction = self.transactions[transaction_id]
        cprint("Transaction %s %s: when %s" % (transaction_id, status, self.when), color=Color.Green)
        is_sender = (self.id == self.state.transaction_sender_id(transaction_id))
        if status == state.TransactionStatus.Canceled:
            transaction.canceled = True
            state.join_transaction(transaction_id)
        elif status == state.TransactionStatus.Rejected:
            transaction.rejected = True
            state.join_transaction(transaction_id)
        elif status == state.TransactionStatus.Failed:
            transaction.failed = True
            state.join_transaction(transaction_id)
        elif not is_sender and status == state.TransactionStatus.RecipientWaitForDecision:
            self.machine_id = state.accept_transaction(transaction_id)
        if (status == transaction.status_dict(reverse = True)[self.when]):
            time.sleep(self.delay / 1000)
            state.cancel_transaction(transaction_id)
            transaction.canceled = True
            state.join_transaction(transaction_id)
