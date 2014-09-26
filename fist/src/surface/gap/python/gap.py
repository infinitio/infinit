# -*- encoding: utf-8 -*-
"""
gap library binding module

>>> state = State
>>> state.meta_status
True
>>> state.register("Testing Program", "test@infinit.io", "MEGABIET", "dev name")
>>>

"""

import _gap


class _State:
    """State is the interface to gap library functions
    """
    read_perm = _gap.gap_read
    write_perm = _gap.gap_write
    exec_perm = _gap.gap_exec

    def __init__(self):
        self.__state = None
        self.email = ''

        directly_exported_methods = [
            'enable_debug',
            'set_device_name',

            'logout',

            # Users.
            'search_users',
            'get_swaggers',
            'user_fullname',
            'user_handle',
            'email',
            'remaining_invitations',
            'user_directory',

            # Transaction
            'send_files',
            'send_files_by_email',
            'set_output_dir',
            'get_output_dir',
            'cancel_transaction',
            'accept_transaction',
            'transactions',
            'transaction_progress',
            'transaction_sender_id',
            'transaction_sender_fullname',
            'transaction_sender_device_id',
            'transaction_recipient_id',
            'transaction_recipient_fullname',
            'transaction_recipient_device_id',
            'transaction_files',
            'transaction_files_count',
            'transaction_total_size',
            'transaction_is_directory',
            'transaction_status',
            'join_transaction',

            # Notifications
            'poll',

            # Callback.
            'transaction_callback',
            'message_callback',
            'user_status_callback',
            'new_swagger_callback',

            # error
            'on_error_callback',
        ]

        def make_method(meth):
            method = lambda *args: (
                self.__call(meth, *args)
            )
            method.__doc__ = getattr(_gap, meth).__doc__
            return method

        for method in directly_exported_methods:
            setattr(self, method, make_method(method))

        self.Status = getattr(_gap, "Status")
        self.TransactionStatus = getattr(_gap, "TransactionStatus")

    @property
    def meta_status(self):
        try:
            return self.__call('meta_status') == self.Status.ok
        except Exception as e:
            return False

    @property
    def has_device(self):
        try:
            return self.__call('device_status') == self.Status.ok
        except:
            return False

    def __call(self, method, *args):
        assert(self.__state != None)
        res = getattr(_gap, method)(self.__state, *args)
        if isinstance(res, _gap.Status) and res != self.Status.ok:
            raise Exception(
                "Error while calling %s: %s " % (method, str(res))
            )
        return res

    def login(self, email, password):
        self.email = email
        pw_hash = self.__call('hash_password', email, password)
        self.__call('login', email, pw_hash)

    @property
    def logged(self):
        return self.__call('is_logged')

    def register(self, fullname, email, password):
        self.email = email
        pw_hash = self.__call('hash_password', email, password)
        self.__call('register', fullname, email, pw_hash)

    @property
    def _id(self):
        return self.__call('_id');

class State(_State):


    def __init__(self, *args, **kwargs):
        self.__args = args
        self.__kwargs = kwargs
        super(State, self).__init__()
        self.__state = None

    def __enter__(self):
        if len(self.__args) == 0 and len(self.__kwargs) == 0:
            print("WARNING, defaulting to devlopment server")
            self.__state = _gap.new(False)
        else:
            self.__state = _gap.new(*self.__args, **self.__kwargs)
        assert self.__state is not None
        return self

    def __exit__(self, exc_type, value, traceback):
        if self.__state is not None:
            _gap.free(self.__state)
        self.__state = None

if __name__ == "__main__":
    import doctest
    doctest.testmod()
