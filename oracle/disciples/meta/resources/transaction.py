
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

filepath = os.path.abspath(os.path.join(os.path.dirname(__file__), 'transaction_status.hh.inc'))

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
              'first_filename': "The first file name",
              'files_count': 32
              'total_size': 42 (ko)
              'is_directory': bool
              'device_id': The device from where the file is get
              'network_id': "The network name", #required
              'message': 'a message to the recipient'
         }
         -> {
                'created_transaction_id':
                'remaining_invitations':
            }


    Errors:
        Using an id that doesn't exist.

    """
    __pattern__ = "/transaction/create"

    _validators = [
        ('recipient_id_or_email', regexp.NonEmptyValidator),
        ('network_id', regexp.NetworkValidator),
        ('device_id', regexp.DeviceIDValidator),
    ]

    _mendatory_fields = [
        ('first_filename', basestring),
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
        first_filename = self.data['first_filename'].strip()
        network_id = self.data['network_id'].strip()
        device_id = self.data['device_id'].strip()

        if not database.networks().find_one(database.ObjectId(network_id)):
            return self.error(error.NETWORK_NOT_FOUND)

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
                    register_status = 'ghost',
                    notifications = [],
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

            'network_id': database.ObjectId(network_id),

            'message': message,

            'first_filename': first_filename,
            'files_count': self.data['files_count'],
            'total_size': self.data['total_size'],
            'is_directory': self.data['is_directory'],

            'timestamp': time.time(),
            'status': PENDING,
            'early_accepted': False,
        }

        transaction_id = database.transactions().insert(transaction)

        sent = first_filename;
        if transaction['files_count'] > 1:
            sent +=  " and %i other files" % (transaction['files_count'] - 1)

        # XXX: MAIL DESACTIVATED
        if not self.connected(recipient_id):
            if not invitee_email:
                invitee_email = database.users().find_one({'_id': database.ObjectId(id_or_email)})['email']

            if new_user:
                meta.invitation.invite_user(
                    invitee_email,
                    mail_template='send-file',
                    reply_to = self.user['email'],
                    filename = first_filename,
                    sendername = self.user['fullname'],
                    user_id = str(self.user['_id']),
                )


        self.notifier.notify_some(
            notifier.TRANSACTION,
            [database.ObjectId(recipient_id), database.ObjectId(_id)], # sender and recipient.
            transaction
        )

        return self.success({
            'created_transaction_id': transaction_id,
            'remaining_invitations': self.user.get('remaining_invitations', 0),
        })

class FullyCreated(Page):
    """
    XXX
    """
    __pattern__ = "/transaction/fully_created"

    _validators = [
        ('transaction_id', regexp.TransactionValidator),
    ]

    def POST(self):
        self.requireLoggedIn()

        status = self.validate()
        if status:
            return self.error(status)

        transaction =  database.transactions().find_one(
            database.ObjectId(self.data['transaction_id'].strip())
        )

        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)

        if self.user['_id'] != transaction['sender_id']:
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

        if transaction['status'] not in (PENDING, ACCEPTED):
            return self.error(
              error.TRANSACTION_OPERATION_NOT_PERMITTED,
              "This transaction can't be %s. Current status : %s" % (
                _status_to_string[CREATED],
                _status_to_string[transaction['status']]
              )
            )

        sender = database.users().find_one(
          database.ObjectId(transaction['sender_id'])
        )

        assert sender is not None

        transaction.update({
            'status': transaction['status'] == ACCEPTED and ACCEPTED or CREATED,
        })

        updated_transaction_id = database.transactions().save(transaction);

        self.notifier.notify_some(
            notifier.TRANSACTION_STATUS,
            [transaction['sender_id'], transaction['recipient_id']],
            {
                'transaction_id': str(updated_transaction_id),

                # Status.
                'status': CREATED,
            }
        )

        return self.success({
            'updated_transaction_id': str(updated_transaction_id),
        })

class Accept(Page):
    """
    Use to accept a file transfer.
    Maybe more in the future but be careful, for the moment, user MUST be the recipient.
    POST {
           'transaction_id' : the id of the transaction.
           'device_id': the device id on which the user accepted the transaction.
           'device_name': the device name on which the user accepted the transaction.
         }
         -> {
                 'updated_transaction_id': the network id or empty string if refused.
            }

    Errors:
        The transaction doesn't exists.
        The use is not the recipient.
        Recipient and sender devices are the same.
    """
    __pattern__ = "/transaction/accept"

    _validators = [
        ('transaction_id', regexp.TransactionValidator),
        ('device_id', regexp.DeviceIDValidator),
        ('device_name', regexp.NonEmptyValidator),
    ]

    def POST(self):
        self.requireLoggedIn()

        status = self.validate()
        if status:
            return self.error(status)

        transaction =  database.transactions().find_one(
            database.ObjectId(self.data['transaction_id'].strip()))

        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)

        if self.user['_id'] != transaction['recipient_id']:
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

        if self.data['device_id'] == transaction['sender_device_id']:
            return self.error(
              error.TRANSACTION_CANT_BE_ACCEPTED,
              "Sender and recipient devices are the same."
            )

        if transaction['status'] not in (PENDING, CREATED):
            return self.error(
                error.TRANSACTION_OPERATION_NOT_PERMITTED,
                "This transaction can't be %s. Current status : %s" % (
                    _status_to_string[ACCEPTED],
                    _status_to_string[transaction['status']]
                )
            )

        sender = database.users().find_one(
          database.ObjectId(transaction['sender_id'])
        )

        # XXX: If the sender delete his account while transaction is pending.
        # We should turn all his transaction to canceled.
        assert sender is not None

        transaction.update({
            'recipient_fullname': self.user['fullname'],
            'recipient_device_name' : self.data['device_name'],
            'recipient_device_id': database.ObjectId(self.data['device_id']),
            'status': ACCEPTED,
            'early_accepted': bool(transaction['status'] == PENDING)
        })

        updated_transaction_id = database.transactions().save(transaction);

        # If transfer is accepted, increase popularity of each user.
        if self.user['_id'] != sender['_id']:
            # XXX: probably not optimized, we should maybe use database.find_and_modify and increase
            # the value.
            sender['swaggers'][str(self.user['_id'])] = sender['swaggers'].setdefault(str(self.user['_id']), 0) + 1;
            self.user['swaggers'][str(sender['_id'])] = self.user['swaggers'].setdefault(str(sender['_id']), 0) + 1;
            database.users().save(sender)
            database.users().save(self.user)

        self.notifier.notify_some(
            notifier.TRANSACTION_STATUS,
            [transaction['sender_id'], transaction['recipient_id']],
            {
                'transaction_id': str(updated_transaction_id),

                # Status.
                'status': ACCEPTED,
            }
        )

        return self.success({
            'updated_transaction_id': str(updated_transaction_id),
        })

class Prepare(Page):
    """
    Notify the user that everything is ok and he can prepare downloading file.
    POST {
             'transaction_id' : the id
         }
         -> {
                 'updated_transaction_id' : the (new) id
            }

    Errors:
        The transaction doesn't exist.
        The user is not the sender.
        The transaction is not in a state in which it can be prepareed.
    """
    __pattern__ = "/transaction/prepare"

    _validators = [
        ('transaction_id', regexp.TransactionValidator),
    ]

    def POST(self):
        self.requireLoggedIn()

        status = self.validate()
        if status:
            return self.error(status)

        transaction =  database.transactions().find_one(
            database.ObjectId(self.data['transaction_id'].strip())
        )

        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)

        if self.user['_id'] != transaction['sender_id']:
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

        if transaction['status'] != ACCEPTED:
            return self.error(
                error.TRANSACTION_OPERATION_NOT_PERMITTED,
                "This transaction can't be %s. Current status : %s" % (
                    _status_to_string[PREPARED],
                    _status_to_string[transaction['status']]
                )
            )

        transaction.update({
            'status': PREPARED, # prepareed.
        })

        updated_transaction_id = database.transactions().save(transaction)

        self.notifier.notify_some(
            notifier.TRANSACTION_STATUS,
            [transaction['sender_id'], transaction['recipient_id']],
            {
                'transaction_id': str(updated_transaction_id),

                # Status.
                'status': PREPARED,
            }
        )

        return self.success({
            'updated_transaction_id': str(updated_transaction_id),
        })

class Start(Page):
    """
    Notify the user that everything is ok and he can start downloading file.
    POST {
             'transaction_id' : the id
         }
         -> {
                 'updated_transaction_id' : the (new) id
            }

    Errors:
        The transaction doesn't exist.
        The user is not the sender.
        The transaction is not in a state in which it can be started.
    """
    __pattern__ = "/transaction/start"

    _validators = [
        ('transaction_id', regexp.TransactionValidator),
    ]

    def POST(self):
        self.requireLoggedIn()

        status = self.validate()
        if status:
            return self.error(status)

        transaction =  database.transactions().find_one(
            database.ObjectId(self.data['transaction_id'].strip()))

        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)

        if self.user['_id'] != transaction['recipient_id']:
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

        if transaction['status'] != PREPARED:
            return self.error(error.TRANSACTION_OPERATION_NOT_PERMITTED,
                              "This transaction can't be %s. Current status : %s"
                              % (_status_to_string[STARTED], _status_to_string[transaction['status']])
            )

        network = database.networks().find_one({
            '_id': database.ObjectId(transaction["network_id"]),
        })
        if not network:
            return self.forbidden("Couldn't find any network with this id")
        if network['owner'] != self.user['_id'] and \
           self.user['_id'] not in network['users']:
            return self.forbidden("This network does not belong to you")

        send_endpoints = network["nodes"][str(transaction["sender_device_id"])]
        recv_endpoints = network["nodes"][str(transaction["recipient_device_id"])]

        ap_endpoint = self.apertus.add_link(str(transaction["network_id"]),
                send_endpoints["externals"],
                recv_endpoints["externals"])

        # Add the current apertus endpoint to the externals adddresses of the
        # devices.
        ip, port = ap_endpoint.split(":")
        send_endpoints["fallback"] = [{"ip" : ip, "port" : port}]
        recv_endpoints["fallback"] = [{"ip" : ip, "port" : port}]

        updated_network_id = database.networks().save(network)

        transaction.update({
            'status': STARTED, # started.
        })
        updated_transaction_id = database.transactions().save(transaction)

        self.notifier.notify_some(
            notifier.TRANSACTION_STATUS,
            [transaction['sender_id'], transaction['recipient_id']],
            {
                'transaction_id': str(updated_transaction_id),

                # Status.
                'status': STARTED,
            }
        )


        return self.success({
            'updated_transaction_id': str(updated_transaction_id),
        })

class Finish(Page):
    """
    Notify the sender that the transaction is complete.
    POST {
             'transaction_id' : the id
         }
         -> {
                 'updated_transaction_id' : the (new) id
            }

    Errors:
        The transaction doesn't exist.
        The user is not the recipient.
    """
    __pattern__ = "/transaction/finish"

    _validators = [
        ('transaction_id', regexp.TransactionValidator)
    ]

    def POST(self):
        self.requireLoggedIn()

        status = self.validate()
        if status:
            return self.error(status)

        transaction =  database.transactions().find_one(
            database.ObjectId(self.data['transaction_id'].strip()))

        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)

        if self.user['_id'] != transaction['recipient_id']:
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

        if transaction['status'] != STARTED :
            return self.error(error.TRANSACTION_OPERATION_NOT_PERMITTED,
                              "This transaction can't be %s. Current status : %s"
                              % (_status_to_string[FINISHED], _status_to_string[transaction['status']])
            )

        transaction.update({
            'status': FINISHED, # finished.
        })

        # This transaction should be move to a 'finished_transaction' collection.
        updated_transaction_id = database.transactions().save(transaction);

        self.notifier.notify_some(
            notifier.TRANSACTION_STATUS,
            [transaction['sender_id'], transaction['recipient_id']],
            {
                'transaction_id': str(transaction['_id']),

                # Status.
                'status': FINISHED,
            }
        )

        network = database.networks().find_one({
            '_id' : database.ObjectId(transaction["network_id"])
        })
        if not network:
            return self.forbidden("Couldn't find any network with this id")
        if network['owner'] != self.user['_id'] and \
           self.user['_id'] not in network['users']:
            return self.forbidden("This network does not belong to you")

        send_endpoints = network["nodes"][str(transaction["sender_device_id"])]
        recv_endpoints = network["nodes"][str(transaction["recipient_device_id"])]

        self.apertus.del_link(
            str(transaction["network_id"]),
            send_endpoints["externals"],
            recv_endpoints["externals"]
        )

        return self.success({
            'updated_transaction_id': str(updated_transaction_id),
        })


class Cancel(Page):
    """
    Delete the transaction that has been rejected or you may want to delete it by your self.
    POST {
              'transaction_id': the id
         }
         -> {
                 'updated_transaction_id'
            }

    Errors:
        The transaction doesn't exist.
        The user is not the sender.
        The transaction is not in a state in which it can be deleted.
    """
    __pattern__ = "/transaction/cancel"

    _validators = [
        ('transaction_id', regexp.TransactionValidator)
    ]

    def POST(self):
        self.requireLoggedIn()

        status = self.validate()
        if status:
            return self.error(status)

        transaction =  database.transactions().find_one(
            database.ObjectId(self.data['transaction_id'].strip()))

        if not transaction:
            return self.error(error.TRANSACTION_DOESNT_EXIST)

        if transaction['status'] == CANCELED:
            return self.success({'updated_transaction_id': str(transaction['_id']) })

        if self.user['_id'] not in (transaction['sender_id'], transaction['recipient_id']):
            return self.error(error.TRANSACTION_DOESNT_BELONG_TO_YOU)

        if not transaction['status'] in (ACCEPTED, PENDING, STARTED, PREPARED, CREATED):
            return self.error(error.TRANSACTION_OPERATION_NOT_PERMITTED,
                              "This transaction can't be %s. Current status : %s"
                              % (_status_to_string[CANCELED], _status_to_string[transaction['status']])
            )

        transaction.update({
            'status': CANCELED, # canceled.
        })

        updated_transaction_id = database.transactions().save(transaction)

        self.notifier.notify_some(
            notifier.TRANSACTION_STATUS,
            [transaction['sender_id'], transaction['recipient_id']],
            {
                'transaction_id': str(transaction['_id']),

                # Status.
                'status': CANCELED,
            }
        )
        network = database.networks().find_one({
            '_id' : database.ObjectId(transaction["network_id"])
        })

        if transaction['recipient_device_id'] == "":
            return self.success({'updated_transaction_id': str(updated_transaction_id), })

        if not network:
            return self.forbidden("Couldn't find any network with this id")

        if network['owner'] != self.user['_id'] and \
           self.user['_id'] not in network['users']:
            return self.forbidden("This network does not belong to you")

        send_endpoints = getattr(network["nodes"], str(transaction["sender_device_id"]), [])
        recv_endpoints = getattr(network["nodes"], str(transaction["recipient_device_id"]), [])

        self.apertus.del_link(str(transaction["network_id"]),
                              getattr(send_endpoints, "externals", []),
                              getattr(recv_endpoints, "externals", []),
                              )

        return self.success({
            'updated_transaction_id': str(updated_transaction_id),
        })

class All(Page):
    """
    Get all transaction involving user (as sender or recipient).
    GET
         {
             # optionnal
             'transaction_filter' : CANCELED
         }
         -> {
                 [id0, id1, ...]
            }
    """
    __pattern__ = "/transactions"

    def GET(self):
        self.requireLoggedIn()

        filter_ = [CANCELED, FINISHED]  #'transaction_filter' in self.data and self.data['transaction_filter'] or CANCELED

        transactions = list(database.transactions().find(
            {
                '$or':
                    [
                        {'recipient_id': self.user['_id']},
                        {'sender_id': self.user['_id']}
                    ],
                'status':
                    {
                        '$nin': filter_
                    }
            },
            ['_id'] #Take only id field.
        ))

        transactions = [str(t['_id']) for t in transactions]

        return self.success({'transactions': transactions})

class One(Page):
    """
    Get details of a specific transaction.
        Get
            -> {
                    'transaction_id' : The id of the transaction.

                    'sender_id' :
                    'sender_fullname' :
                    'sender_device_id' :

                    'recipient_id' :
                    'recipient_fullname' :
                    'recipient_device_id' :
                    'recipient_device_name' :

                    'network_id' :

                    'first_filename' :
                    'files_count' :
                    'total_size' :
                    'is_directory' :

                    'status' :
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
