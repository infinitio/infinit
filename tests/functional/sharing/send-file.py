#!/usr/bin/env python3

import gap
import random
import hashlib

import os
import time
import tempfile

def _file_sha1(file):
    sha1 = hashlib.sha1()
    while True:
        data = file.read(4096)
        if not data:
            break
        sha1.update(data)
    return sha1.hexdigest()

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
                 in_directory = False,
                 **kw):
        self.size = size
        self.random_ratio = random_ratio
        if in_directory: # Update the destination.
            destination = os.path.join(tempfile.gettempdir(), str(size))
            import shutil
            if os.path.isdir(destination):
                shutil.rmtree(destination)
            os.makedirs(destination)
            kw['dir'] = destination
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

class Transaction:

    def __init__(self, state, id):
        assert isinstance(id, str)
        self.id = id
        self.state = state
        self.localy_accepted = False

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

    @property
    def status(self):
        return self.state.transaction_status(self.id)

    def __str__(self):
        return "Transaction(%s, %s, localy_accepted: %s)" % (self.id, self.status, self.localy_accepted)

    def __repr__(self):
        return "Transaction(%s, %s, localy_accepted: %s)" % (self.id, self.status, self.localy_accepted)

class User:
    def __init__(self,
                 meta_server = None,
                 trophonius_server = None,
                 fullname = None,
                 email = None,
                 register = False,
                 auto_accept = True,
                 output_dir = None):
        assert meta_server is not None
        assert trophonius_server is not None
        self.meta_server = meta_server
        self.trophonius_server = trophonius_server
        self.state = None
        self.email = email
        self.fullname = fullname
        self.register = register
        self.auto_accept = auto_accept
        self.output_dir = output_dir
        self.use_temporary = self.output_dir is None
        self.temporary_output_dir = None
        self.finished = False
        self.accepted_transactions=set()

    def __enter__(self):
        # state setup
        assert self.state == None
        self.state = gap.State(
            "localhost", int(self.meta_server.meta_port),
            "0.0.0.0", int(self.trophonius_server.port)
        )
        self.state.__enter__()

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

        self.id = self.state._id
        # output directory setup
        if self.use_temporary:
            assert self.temporary_output_dir is None
            self.temporary_output_dir = tempfile.TemporaryDirectory()
            self.temporary_output_dir.__enter__()
            self.output_dir = self.temporary_output_dir.name
        if not os.path.exists(self.output_dir):
            os.path.makedirs(self.output_dir)
        assert os.path.isdir(self.output_dir)

        self.state.set_output_dir(self.output_dir)

        self.transactions = dict()
        self.state.transaction_callback(self._on_transaction)

    def __exit__(self, type, value, traceback):
        # state cleanup
        try:
            if self.state is not None:
                self.state.logout()
                self.state.__exit__(None, None, None)
        finally:
            self.state = None

        # output directory cleanup
        try:
            if self.use_temporary:
                assert self.temporary_output_dir is not None
                self.temporary_output_dir.__exit__(None, None, None)
                self.temporary_output_dir = None
        except:
            print(
                "Couldn't remove", self.output_dir, "output directory",
                file = sys.stderr
            )

    def send_files(self, recipient = None, files = None):
        assert isinstance(recipient, User)
        assert isinstance(files, list)
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
        elif not is_sender and status == state.TransactionStatus.created and \
             not self.transactions[transaction_id].localy_accepted:
            self.transactions[transaction_id].localy_accepted = True
            state.accept_transaction(transaction_id)
        elif status == state.TransactionStatus.finished:
            self.finished = True

class TransferReactor:
    def __init__(self, sender = None, recipient = None, files = None):
        assert sender is not None
        assert recipient is not None
        assert files is not None
        assert isinstance(files, list)
        self.sender = sender
        self.recipient = recipient
        self.files = files
        self.name = " ".join(self.files)

    def run(self, timeout = 300):
        assert len(self.sender.transactions) == 0
        assert len(self.recipient.transactions) == 0

        expected_files = [
            os.path.join(self.recipient.output_dir,
                         os.path.basename(file))
            for file in self.files]

        print("Sender file", self.files)
        print("Recipient file", expected_files)

        self.sender.send_files(files = self.files, recipient = self.recipient)
        start = time.time()

        transaction_finished = False
        transaction = None
        while not (sender.finished and recipient.finished) and \
                time.time() - start < timeout:
            time.sleep(0.5)
            self.poll()
            time.sleep(0.1)
            if len(self.recipient.transactions):
                assert len(self.recipient.transactions) == 1
                transaction_id = list(self.recipient.transactions.keys())[0]
                transaction = self.recipient.transactions[transaction_id]
                # ?? assert transaction_id in self.sender.transactions
            if transaction is not None:
                if transaction.status == "finished":
                    print('$' * 80)
                    print("@@@@ FINISHED")
                    transaction_finished = True
                    break
                print('#' * 80, transaction.progress)

        for file, expected_file in zip(self.files, expected_files):
            if os.path.exists(expected_file):
                print('$' * 80)
                print("@@@@ Found expected file %s!" % expected_file)
                if file_sha1(file) != file_sha1(expected_file):
                    raise TestFailure("Wrong SHA1")
                return True
        raise TestFailure(self.name)

    def poll(self):
        self.sender.state.poll()
        self.recipient.state.poll()

class TestFailure(Exception):
    def __init__(self, *args, **kwargs):
        super().__init__(args, kwargs)

if __name__ == '__main__':
    # XXX: For the moment, there is an interdependence between meta and tropho.
    # As long as it stands, we need to have a control port.
    trophonius_control_port = 39074

    import utils
    with utils.Servers() as (meta, trophonius, apertus):
        sender, recipient = (
            User(
                meta_server = meta,
                trophonius_server = trophonius,
                fullname = "sender",
                email = "sender@infinit.io",
                register = True
                ),
            User(
                meta_server = meta,
                trophonius_server = trophonius,
                fullname = "recipient",
                email = "recipient@infinit.io",
                register = True,
                ),
        )

        cases = [
            RandomTempFile(40),
            RandomTempFile(1000, in_directory = True),
            RandomTempFile(4000000),
            RandomTempFile(400000000),
            [RandomTempFile(30), RandomTempFile(300)],
        ]
        for item in cases:
            files = isinstance(item, list) and [file.name for file in item] or [item.name]
            with sender, recipient:
                TransferReactor(
                    sender = sender,
                    recipient = recipient,
                    files = files,
                ).run(timeout = 30)
