#!/usr/bin/env python3
# -*- encoding: utf-8 --*

import gap
import random
import hashlib

import os
import time
import tempfile

import string
def generator(size = 12, prefix = "", chars = string.ascii_lowercase):
    return prefix + ''.join(random.choice(chars) for x in range(size))

def email_generator(size = 12, prefix = "", chars = string.ascii_lowercase):
    return generator(size, prefix, chars) + "@infinit.io"

class TestFailure(Exception):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

def _file_sha1(file):
    sha1 = hashlib.sha1()
    while True:
        data = file.read(4096)
        if not data:
            break
        sha1.update(data)
    return sha1.hexdigest()

def dir_sha1(dst, src):
    assert os.path.isdir(dst)
    assert os.path.isdir(src)

    src_dir, src_dirs, src_files = list(os.walk(src))[0]
    dst_dir, dst_dirs, dst_files = list(os.walk(dst))[0]
    src_files.sort()
    dst_files.sort()
    for dst_file, src_file in zip(dst_files, src_files):
        dst_file = os.path.join(dst_dir, dst_file)
        src_file = os.path.join(src_dir, src_file)
        if not os.path.exists(dst_file):
            raise TestFailure("{} does not exists".format(dst_file))
        if not os.path.exists(src_file):
            raise TestFailure("{} does not exists".format(src_file))
        if file_sha1(dst_file) != file_sha1(src_file):
            raise TestFailure("sha1({}) != sha1({})".format(dst_file, src_file))

def file_sha1(file):
    if isinstance(file, str):
        with open(file, 'rb') as handle:
            return _file_sha1(handle)
    else:
        old_pos = file.tell()
        try:
            return _file_sha1(file)
        finally:
            file.seek(old_pos)

class RandomTempFile:
    def __init__(self,
                 size = 0,
                 random_ratio : "How many bytes are randomized" = 0.1,
                 **kw):
        self.size = size
        self.random_ratio = random_ratio
        self.file = tempfile.NamedTemporaryFile(**kw)
        if self.size > 0:
            self.file.truncate(self.size)
        if self.random_ratio:
            to_randomize = self.size * self.random_ratio
            randomized = 0
            while randomized < to_randomize:
                pos = random.randint(0, self.size - 1)
                self.file.seek(pos)
                to_write = min(self.size - 1 - pos, random.randint(0, 4096))
                if to_write <= 0:
                    continue
                data = bytes(chr(random.randint(1, 255)), 'utf8') * to_write
                self.file.write(data[:to_write])
                randomized += to_write
        self.file.seek(0)
        assert os.path.getsize(self.file.name) == self.size
        self._sha1 = None

    def __enter__(self):
        self.file.__enter__()
        return self

    def __exit__(self, type, value, traceback):
        self.file.__exit__(type, value, traceback)

    def __getattr__(self, attr):
        return getattr(self.file, attr)

    @property
    def sha1(self):
        if self._sha1 is None:
            self._sha1 = file_sha1(self.file)
        return self._sha1

class RandomDirectory(tempfile.TemporaryDirectory):
    def __init__(self, file_count = 15, min_file_size = 1024 * 1024, max_file_size = 1024 * 1024 * 10):
        if min_file_size > max_file_size: min_file_size, max_file_size = max_file_size, min_file_size
        tempfile.TemporaryDirectory.__init__(self, prefix="tmpdir-")
        self.files = [RandomTempFile(random.randint(min_file_size, max_file_size), dir=self.name) for x in range(file_count)]

    def __enter__(self):
        super().__enter__()
        for f in self.files:
            f.__enter__()
        return self

    def __exit__(self, exception_type, exception, bt):
        for f in self.files:
            f.__exit__(exception_type, exception, bt)
        super().__exit__(exception_type, exception, bt)

    def __del__(self):
        for f in self.files:
            f.__del__()
        super().__del__()

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
        import traceback
        print(type, value, traceback.print_tb(tb))
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
            print("@@@@ expect file", file, expected_file)
            if os.path.exists(expected_file):
                print('$' * 80)
                print("@@@@ Found expected file %s!" % expected_file)
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

        print("Sender file", self.files)
        print("Recipient file", expected_files)

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
            if len(self.recipient.transactions):
                assert len(self.recipient.transactions) == 1
                transaction_id = list(self.recipient.transactions.keys())[0]
                transaction = self.recipient.transactions[transaction_id]
                if sender.transactions[transaction_id].finished and recipient.transactions[transaction_id].finished:
                    break
            if transaction is not None:
                if transaction.status == "finished":
                    print('$' * 80)
                    print("@@@@ FINISHED")
                    transaction_finished = True
                    break
                sender_progress = self.sender.transactions[transaction_id].progress
                recipient_progress = self.recipient.transactions[transaction_id].progress
                print('#' * (80 - len(str(sender_progress)) - 2), sender_progress)
                print('%' * (80 - len(str(recipient_progress)) - 2), recipient_progress)

        self.verify_transfer(expected_files)
        return True

class GhostScenario(Scenario):
    def __init__(self, sender = None, recipient = None, files = None):
        super().__init__(sender, files)
        assert isinstance(sender, User)
        assert isinstance(recipient, GhostUser)
        self.recipient = recipient

    def run(self, timeout = 300):
        print("Sender file", self.files)

        self.sender.send_files(files = self.files, email = recipient.email)
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
                print("boite %s" % transaction.status)
                if transaction.status == "created":
                    break

        transaction = None
        transaction_finished = False
        self.recipient._register_or_login()
        self.users.append(self.recipient)

        expected_files = [os.path.join(
                self.recipient.output_dir, os.path.basename(file))
                          for file in self.files]
        print("Recipient file", expected_files)

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
                if sender.transactions[transaction_id].finished and recipient.transactions[transaction_id].finished:
                    break
            if transaction is not None:
                if transaction.status == "finished":
                    print('$' * 80)
                    print("@@@@ FINISHED")
                    transaction_finished = True
                    break
                sender_progress = self.sender.transactions[transaction_id].progress
                #recipient_progress = self.recipient.transactions[transaction_id].progress
                print('#' * (80 - len(str(sender_progress)) - 2), sender_progress)
                #print('%' * (80 - len(str(recipient_progress)) - 2), recipient_progress)

        self.verify_transfer(expected_files)

        return True

if __name__ == '__main__':

    cases = [
        [RandomTempFile(4)] * 10,
        RandomTempFile(400),
        RandomDirectory(file_count = 512, min_file_size = 10, max_file_size = 1024),
        [RandomDirectory(file_count = 10, min_file_size = 128, max_file_size = 2048), RandomTempFile(100), RandomTempFile(40000)],
    ]

    import utils
    with utils.Servers() as (meta, trophonius, apertus):
        # Default case.
        sender, recipient = (User(meta_server = meta,
                                  trophonius_server = trophonius,
        apertus_server = apertus,
                                  register = True),
                             User(meta_server = meta,
                                  trophonius_server = trophonius,
        apertus_server = apertus,
                                  register = True))

        for item in cases:
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                DefaultScenario(sender = sender,
                                recipient = recipient,
                                files = files).run(timeout = 1024 * 1024)

        # Default early accept.
        sender, recipient = (User(meta_server = meta,
                                  trophonius_server = trophonius,
        apertus_server = apertus,
                                  register = True),
                             User(meta_server = meta,
                                  trophonius_server = trophonius,
        apertus_server = apertus,
                                  register = True,
                                  early_accept = True))

        for item in cases:
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                DefaultScenario(sender = sender,
                                recipient = recipient,
                                files = files).run(timeout = 1024 * 1024)

        # Forwarder.
        os.environ["INFINIT_LOCAL_ADDRESS"] = "128.128.83.31"
        sender, recipient = (User(meta_server = meta,
                                  trophonius_server = trophonius,
                                  apertus_server = apertus,
                                  register = True),
                             User(meta_server = meta,
                                  trophonius_server = trophonius,
                                  apertus_server = apertus,
                                  register = True))

        for item in cases:
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                DefaultScenario(sender = sender,
                                recipient = recipient,
                                files = files).run(timeout = 1024 * 1024)

        os.environ.pop("INFINIT_LOCAL_ADDRESS")

        # Invitations.
        # XXX: 3 invitations, cases should be <= 3.
        for item in cases[0:3]:
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                DefaultScenario(sender = sender,
                                recipient = recipient,
                                files = files).run(timeout = 1024 * 1024)

        sender, recipient = (User(meta_server = meta,
                                  trophonius_server = trophonius,
                                  apertus_server = apertus,
                                  register = True),
                             GhostUser(meta_server = meta,
                                       trophonius_server = trophonius,
                                       apertus_server = apertus))

        for item in cases:
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                GhostScenario(sender = sender,
                              recipient = recipient,
                              files = files).run(timeout = 1024 * 1024)
