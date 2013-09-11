# -*- encoding: utf-8 -*-

import json
import web
import os.path
import sys
import time

from meta.page import Page
from meta import notifier
from meta import database
from meta import error
from meta import regexp
from meta import mail
import meta.invitation

import re

import metalib

# Possible transaction status.
# XXX: Should be change to a bitfield to improve filter in the Getter.
# XXX: Should also be defined in metalib.

_macro_matcher = re.compile(r'(.*\()(\S+)(,.*\))')

def replacer(match):
    field = match.group(2)
    return match.group(1) + "'" + field + "'" + match.group(3)

_status_to_string = dict();

def TRANSACTION_STATUS(name, value):
    globals()[name.upper()] = value
    _status_to_string[value] = str(name)

filepath = os.path.abspath(
  os.path.join(os.path.dirname(__file__), 'transaction_status.hh.inc')
)

configfile = open(filepath, 'r')
for line in configfile:
    eval(_macro_matcher.sub(replacer, line))

class Create(Page):
    """
    Send a file to a specific user.
    If you pass an email and the user is not registred in infinit,
    create a 'ghost' in the database, waiting for him to register.

    POST {
        'recipient_id_or_email': "email@pif.net", #required
        'files': ["The first file name", "file2"],
        'files_count': 32
        'total_size': 42 (ko)
        'is_directory': bool
        'device_id': The device from where the file is get
        'message': 'a message to the recipient'
     }
     -> {
        'created_transaction_id': <string>,
        'remaining_invitations': <int>
    }

    Errors:
        Using an id that doesn't exist.
    """
    __pattern__ = "/transaction/create"

    _validators = [
        ('recipient_id_or_email', regexp.NonEmptyValidator),
        ('device_id', regexp.DeviceIDValidator),
    ]

    _mendatory_fields = [
        ('files', list),
        ('files_count', int),
        ('total_size', int),
        ('is_directory', int),
#        ('message', str)
    ]

    def POST(self):
        self.requireLoggedIn()

        status = self.validate()
        if status:
            return self.error(status)

        message = 'message' in self.data and self.data['message'] or ""

        id_or_email = self.data['recipient_id_or_email'].strip().lower()
        files = self.data['files']
        device_id = self.data['device_id'].strip()

        if not database.devices().find_one(database.ObjectId(device_id)):
            return self.error(error.DEVICE_NOT_FOUND)

        new_user = False
        is_ghost = False
        invitee = 0
        invitee_email = ""

        # Determine if user sent a mail or an id.
        if re.match(regexp.Email, id_or_email): # email case.
            invitee_email = id_or_email
            # Check is user is in database.
            recipient = database.users().find_one({'email': id_or_email})
            # if the user doesn't exist, create a ghost and invite.
            if not recipient:
                if self.user.get('remaining_invitations', 0) <= 0:
                    return self.error(error.NO_MORE_INVITATION)
                self.user['remaining_invitations'] -= 1
                database.users().save(self.user)
                new_user = True
                recipient_id = database.users().save({})
                self.registerUser(
                    _id = recipient_id,
                    email = invitee_email,
                    fullname = invitee_email[0:invitee_email.index('@')],
                    register_status = 'ghost',
                    notifications = [],
                    networks = [],
                    swaggers = {},
                    accounts=[{'type':'email', 'id':invitee_email}]
                )
                recipient_fullname = id_or_email
            else:
                recipient_id = recipient['_id']
                recipient_fullname = recipient['register_status'] == 'ghost' and recipient['email'] or recipient['fullname']
        elif re.match(regexp.ID, id_or_email): # id case.
             recipient_id = database.ObjectId(id_or_email)
             recipient = database.users().find_one(recipient_id)
             if recipient is None:
                 return self.error(error.USER_ID_NOT_VALID)
             recipient_fullname = recipient['register_status'] == 'ghost' and recipient['email'] or recipient['fullname']
        else:
            return self.error(error.USER_ID_NOT_VALID)

        _id = self.user['_id']

        transaction = {
            'sender_id': database.ObjectId(_id),
            'sender_fullname': self.user['fullname'],
            'sender_device_id': database.ObjectId(device_id),

            'recipient_id': database.ObjectId(recipient_id),
            'recipient_fullname': recipient_fullname,

            # Empty until accepted.
            'recipient_device_id': '',
            'recipient_device_name': '',

            'message': message,

            'files': files,
            'files_count': self.data['files_count'],
            'total_size': self.data['total_size'],
            'is_directory': self.data['is_directory'],

            'ctime': time.time(),
            'mtime': time.time(),
            'status': CREATED,
        }

        transaction_id = database.transactions().insert(transaction)

        from meta.resources import user
        if not user.is_connected(database.ObjectId(recipient_id)):
            if not invitee_email:
                invitee_email = database.users().find_one(
                    {'_id': database.ObjectId(id_or_email)}
                )['email']

            if new_user:
                meta.invitation.invite_user(
                    invitee_email,
                    source = self.user['_id'],
                    mail_template = 'send-file',
                    reply_to = self.user['email'],
                    filename = files[0],
                    sendername = self.user['fullname'],
                    user_id = str(self.user['_id']),
                )

        from user import increase_swag
        increase_swag(self.user['_id'], recipient_id, self.notifier)

        return self.success({
            'created_transaction_id': transaction_id,
            'remaining_invitations': self.user.get('remaining_invitations', 0),
        })

class Update(Page):
    """
    """
    __pattern__ = "/transaction/update"

    def validate_ownership(self, transaction):
        # Check that user has rights on the transaction
        is_sender = self.user['_id'] == transaction['sender_id']
        is_receiver = self.user['_id'] == transaction['recipient_id']
        if not (is_sender or is_receiver):
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)
        return is_sender

    def on_accept(self, transaction):
        if 'device_id' not in self.data or 'device_name' not in self.data:
            return self.error(
                error.TRANSACTION_OPERATION_NOT_PERMITTED,
                "'device_id' or 'device_name' key is missing")

        device_id = database.ObjectId(self.data["device_id"])
        if device_id not in self.user['devices']:
            return self.error(error.DEVICE_NOT_VALID)

        transaction.update({
                'recipient_fullname': self.user['fullname'],
                'recipient_device_name' : self.data['device_name'],
                'recipient_device_id': device_id,
                })

    def update_status(self, transaction, status, is_sender):
        final_status = [REJECTED, CANCELED, FAILED, FINISHED]
        if transaction['status'] in final_status:
            return self.error(
                error.TRANSACTION_ALREADY_FINALIZED,
                "Cannot change status from %s to %s." % (transaction['status'], status)
                )

        if transaction['status'] == status:
            return self.error(
                error.TRANSACTION_ALREADY_HAS_THIS_STATUS,
                "Cannont change status from %s to %s." % (transaction['status'], status))

        transitions = {
            CREATED: {True: [INITIALIZED, CANCELED, FAILED], False: [ACCEPTED, REJECTED, FAILED]},
            INITIALIZED: {True: [CANCELED, FAILED], False: [ACCEPTED, REJECTED, CANCELED, FAILED]},
            ACCEPTED: {True: [CANCELED, FAILED], False: [FINISHED, CANCELED, FAILED]},
            # FINISHED: {True: [], False: []},
            # CANCELED: {True: [], False: []},
            # FAILED: {True: [], False: []},
            # REJECTED: {True: [], False: []}
        }

        if status not in transitions[transaction['status']][is_sender]:
            return self.error(
                error.TRANSACTION_OPERATION_NOT_PERMITTED,
                "Cannot change status from %s to %s." % (transaction['status'], self.data['status'])
                )

        from functools import partial

        callbacks = {
            INITIALIZED: None,
            ACCEPTED: partial(self.on_accept, transaction),
            READY: None,
            FINISHED: None,
            CANCELED: None,
            FAILED: None,
            REJECTED: None
        }

        cb = callbacks[status]
        if cb is not None:
            cb()

        transaction['status'] = status
        transaction['mtime'] = time.time()
        database.transactions().save(transaction)

        device_ids = [transaction['sender_device_id']]
        recipient_ids = None
        if isinstance(transaction['recipient_device_id'], database.ObjectId):
            device_ids.append(transaction['recipient_device_id'])
        else:
            recipient_ids = [transaction['recipient_id']]

        self.notifier.notify_some(
            notifier.TRANSACTION,
            device_ids = device_ids,
            recipient_ids = recipient_ids,
            message = transaction,
            store = False,
       )

    def POST(self):
        self.requireLoggedIn()

        # Find the transaction
        tr_id = database.ObjectId(self.data['transaction_id'])
        transaction = database.transactions().find_one(tr_id)
        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)

        is_sender = self.validate_ownership(transaction)

        # Check input fied
        if 'status' not in self.data:
            return self.error(
                error.TRANSACTION_OPERATION_NOT_PERMITTED,
                "'status' key is missing")

        self.update_status(transaction, self.data['status'], is_sender)

        return self.success({
            'updated_transaction_id': transaction['_id'],
        })

class All(Page):
    """
    Get all transaction involving user (as sender or recipient).
    GET
    -> {
        [id0, id1, ...]
    }
    """
    __pattern__ = "/transactions"

    def POST(self):
        self.requireLoggedIn()

        filter_ = self.data.get('filter', [CANCELED, FINISHED, FAILED, CREATED, REJECTED])
        inclusive = self.data.get('type', False)
        limit = min(int(self.data.get('count', 100)), 100)



        transaction_ids = list(
            t['_id'] for t in database.transactions().find({
                    '$query': {
                        '$or':[
                            {'recipient_id': self.user['_id']},
                            {'sender_id': self.user['_id']}
                        ],
                        'status': {
                            '$%s' % (inclusive and 'in' or 'nin'): filter_,
                        },
                    },
                },
                fields = ['_id'],
                limit = limit,
                sort = [
                    ('mtime', -1),
                ],
            )
        )

        return self.success({
            'transactions': transaction_ids,
        })

class One(Page):
    """
    Fetch one transaction.
        GET /transaction/_id/view
        -> {
            '_id' : The id of the transaction.
            'sender_id' :
            'sender_fullname' :
            'sender_device_id' :

            'recipient_id' :
            'recipient_fullname' :
            'recipient_device_id' :
            'recipient_device_name' :

            'files' :
            'files_count' :
            'total_size' :
            'is_directory' :

            'status' : <TransactionStatus>,
        }
    """

    __pattern__ = "/transaction/(.+)/view"

    def GET(self, _id):
        self.requireLoggedIn()
        transaction = database.transactions().find_one(database.ObjectId(_id))
        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)
        if not self.user['_id'] in (transaction['sender_id'], transaction['recipient_id']):
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)
        return self.success(transaction)

class ConnectDevice(Page):
    """
    Connect the device to a transaction (setting ip and port)
    POST {
        "_id": "the transaction id",
        "device_id": "the device id",

        # Optional local ip, port
        "locals": [{"ip" : 192.168.x.x, "port" : 62014}, ...],

        # optional external address and port
        "externals": [{"ip" : "212.27.23.67", "port" : 62015}, ...],
    }
    ->
    {
        "updated_transaction_id": "the same transaction id",
    }
    """
    __pattern__ = '/transaction/connect_device'

    _validators = [
        ('_id', regexp.Validator(regexp.ID, error.TRANSACTION_ID_NOT_VALID)),
        ('device_id', regexp.Validator(regexp.DeviceID, error.DEVICE_ID_NOT_VALID)),
    ]

    def POST(self):
        self.requireLoggedIn()

        status = self.validate()
        if status:
            return self.error(status)

        transaction_id = database.ObjectId(self.data["_id"])
        device_id = database.ObjectId(self.data["device_id"])

        device = database.devices().find_one(device_id)
        if not device:
            return self.error(error.DEVICE_NOT_FOUND)

        transaction = database.transactions().find_one(transaction_id)

        if device_id not in [transaction['sender_device_id'],
                             transaction['recipient_device_id']]:
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

        local_addresses = self.data.get('locals') # notice the 's'

        node = dict()

        if local_addresses is not None:
            # Generate a list of dictionary ip:port.
            # We can not take the local_addresses content directly:
            # it's not checked before this point. Therefor, it's insecure.
            node['locals'] = [
                {"ip" : v["ip"], "port" : v["port"]}
                for v in local_addresses if v["ip"] != "0.0.0.0"
            ]
        else:
            node['locals'] = []

        external_addresses = self.data.get('externals')

        if external_addresses is not None:
            node['externals'] = [
                {"ip" : v["ip"], "port" : v["port"]}
                for v in external_addresses if v["ip"] != "0.0.0.0"
            ]
        else:
            node['externals'] = []

        node['fallback'] = []

        database.transactions().update({"_id": transaction_id},
                                       {"$set": {"nodes.%s" % (str(device_id),): node}},
                                       multi = False)

        transaction = database.transactions().find_one(transaction_id)

        print("device %s connected to transaction %s as %s" % (device_id, transaction_id, node))

        if len(transaction['nodes']) == 2 and list(transaction['nodes'].values()).count(None) == 0:
            self.notifier.notify_some(
                notifier.PEER_CONNECTION_UPDATE,
                device_ids = list(transaction['nodes'].keys()),
                message = {
                    "transaction_id": str(transaction_id),
                    "devices": list(transaction['nodes'].keys()),
                    "status": True
                },
                store = False,
            )

        return self.success({})


class Endpoints(Page):
    """
    Return ip port for a selected node.
        POST
               {
                'device_id':
                'self_device_id':
               }
            -> {
                'success': True,
                'externals': ['69.69.69.69:38293', '69.69.69.69:38323']
                'locals': ['69.69.69.69:33293', '69.69.69.69:9323']
            }
    """
    __pattern__ = "/transaction/(.+)/endpoints"

    def POST(self, transaction_id):
        self.requireLoggedIn()

        transaction = database.transactions().find_one(database.ObjectId(transaction_id))
        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)

        device_id = self.data['device_id']
        self_device_id = self.data['self_device_id']

        if self.user['_id'] not in [transaction['sender_id'], transaction['recipient_id']]:
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

        if (not self_device_id in transaction['nodes'].keys()) or (not transaction['nodes'][self_device_id]):
            return self.error(error.DEVICE_NOT_FOUND, "you are not not connected to this transaction")

        if (not device_id in transaction['nodes'].keys()) or (not transaction['nodes'][device_id]):
            return self.error(error.DEVICE_NOT_FOUND, "This user is not connected to this transaction")

        res = dict();

        addrs = {'locals': list(), 'externals': list(), 'fallback' : list()}
        user_node = transaction['nodes'][device_id];

        for addr_kind in ['locals', 'externals', 'fallback']:
            for a in user_node[addr_kind]:
                if a and a["ip"] and a["port"]:
                    addrs[addr_kind].append(
                        (a["ip"], str(a["port"])))

        print("addrs is: ", addrs)

        res['externals'] = ["{}:{}".format(*a) for a in addrs['externals']]
        res['locals'] =  ["{}:{}".format(*a) for a in addrs['locals']]
        res['fallback'] = ["{}:{}".format(*a)
                           for a in self.__application__.fallback]

        return self.success(res)
