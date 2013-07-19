#!/usr/bin/env python3.2
# -*- encoding: utf-8 --*

import os
import time

from utils import file_sha1, dir_sha1, TestFailure

class Scenario:
    def __init__(self, sender = None, files = None):
        assert sender is not None
        assert files is not None
        assert isinstance(files, list)
        self.sender = sender
        self.files = files
        self.name = " ".join(self.files)
        self.users = [self.sender]

    def run(self, timeout = 300):
        raise Exception("Default Scenario can't be run")

    def verify_transfer(self, expected_files):
        for file, expected_file in zip(self.files, expected_files):
            if os.path.exists(expected_file):
                if os.path.isdir(expected_file) and os.path.isdir(file):
                    dir_sha1(expected_file, file)
                elif file_sha1(file) != file_sha1(expected_file):
                    raise TestFailure("sha1({}) != sha1({})".format(file, expected_file))
            else:
                raise TestFailure("{} not found".format(expected_file))

    def poll(self):
        for user in self.users:
            user.state.poll()

class DefaultScenario(Scenario):
    def __init__(self, sender = None, recipient = None, files = None):
        from users import User
        super().__init__(sender, files)
        assert recipient is not None
        assert isinstance(sender, User)
        assert isinstance(recipient, User)
        self.recipient = recipient

        self.users.append(self.recipient)

    def run(self, timeout = 300):
        assert len(self.sender.transactions) == 0
        assert len(self.recipient.transactions) == 0

        expected_files = [os.path.join(self.recipient.output_dir, os.path.basename(file))
                              for file in self.files]

        self.sender.send_files(files = self.files, recipient = self.recipient)
        start = time.time()

        transaction_finished = False
        transaction = None
        while True:
            if not (time.time() - start < timeout):
                raise TestFailure("{}: timeout".format(self.name))
            time.sleep(0.5)
            self.poll()
            time.sleep(0.1)
            if len(self.recipient.transactions) and len(self.sender.transactions):
                assert len(self.recipient.transactions) == 1
                assert len(self.sender.transactions) == 1
                transaction_id = list(self.recipient.transactions.keys())[0]
                transaction = self.recipient.transactions[transaction_id]
                if self.sender.transactions[transaction_id].finished and self.recipient.transactions[transaction_id].finished:
                    break
            if transaction is not None:
                if transaction.status == "finished":
                    transaction_finished = True
                    break
                sender_progress = self.sender.transactions[transaction_id].progress
                recipient_progress = self.recipient.transactions[transaction_id].progress

        self.verify_transfer(expected_files)
        return True

class GhostScenario(Scenario):
    def __init__(self, sender = None, recipient = None, files = None):
        from users import User, GhostUser
        super().__init__(sender, files)
        assert isinstance(sender, User)
        assert isinstance(recipient, GhostUser)
        self.recipient = recipient

    def run(self, timeout = 300):

        self.sender.send_files(files = self.files, email = self.recipient.email)
        start = time.time()

        transaction = None
        while True:
            if not (time.time() - start < timeout):
                raise TestFailure("{}: timeout".format(self.name))
            time.sleep(0.5)
            self.poll()
            if len(self.sender.transactions):
                transaction_id = list(self.sender.transactions.keys())[0]
                transaction = self.sender.transactions[transaction_id]
            if transaction is not None:
                if transaction.status == "created":
                    break

        transaction = None
        transaction_finished = False
        self.recipient._register_or_login()
        self.users.append(self.recipient)

        expected_files = [os.path.join(
                self.recipient.output_dir, os.path.basename(file))
                          for file in self.files]

        while True:
            if not (time.time() - start < timeout):
                raise TestFailure("{}: timeout".format(self.name))
            time.sleep(0.5)
            self.poll()
            time.sleep(0.1)
            if len(self.recipient.transactions) and len(self.sender.transactions):
                assert len(self.recipient.transactions) == 1
                assert len(self.sender.transactions) == 1
                transaction_id = list(self.recipient.transactions.keys())[0]
                transaction = self.recipient.transactions[transaction_id]
                if self.sender.transactions[transaction_id].finished and self.recipient.transactions[transaction_id].finished:
                    break
            if transaction is not None:
                if transaction.status == "finished":
                    transaction_finished = True
                    break
                sender_progress = self.sender.transactions[transaction_id].progress
                #recipient_progress = self.recipient.transactions[transaction_id].progress

        self.verify_transfer(expected_files)

        return True

class CancelScenario(DefaultScenario):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def run(self, timeout = 300):
        assert len(self.sender.transactions) == 0
        assert len(self.recipient.transactions) == 0

        expected_files = [os.path.join(self.recipient.output_dir, os.path.basename(file))
                              for file in self.files]

        self.sender.send_files(files = self.files, recipient = self.recipient)

        start = time.time()
        transaction = None
        while True:
            if not (time.time() - start < timeout):
                raise TestFailure("{}: timeout".format(self.name))
            time.sleep(0.5)
            self.poll()
            time.sleep(0.1)
            if len(self.recipient.transactions):
                assert len(self.recipient.transactions) == 1
                transaction_id = list(self.recipient.transactions.keys())[0]
                transaction = self.recipient.transactions[transaction_id]
            if self.sender.transactions[transaction_id].status == "canceled" and self.recipient.transactions[transaction_id].status == "canceled":
                break
