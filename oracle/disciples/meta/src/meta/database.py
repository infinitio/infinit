# -*- encoding: utf-8 -*-

import pymongo

try:
    from pymongo.objectid import ObjectId
except ImportError:
    from bson.objectid import ObjectId

try:
    from pymongo.binary import Binary
except ImportError:
    from bson.binary import Binary

from . import conf

"""
Define database collections and their constraints
"""

_connection = None
def connection(host=None, port=None):
    host = host or conf.MONGO_HOST
    port = port or conf.MONGO_PORT
    global _connection
    if _connection is None:
        _connection = pymongo.Connection(host, port)
    return _connection

_database = None
def database():
    global _database
    if _database is None:
        _database = getattr(connection(), conf.COLLECTION_NAME)
    return _database

# collections
_users = None
def users():
    global _users
    if _users is None:
        _users = database()['users']
        # constraints
        _users.ensure_index(
            'email', pymongo.ASCENDING,
            kwags={'unique': True}
        )
    return _users

_devices = None
def devices():
    global _devices
    if _devices is None:
        _devices = database()['devices']
    return _devices

_sessions = None
def sessions():
    global _sessions
    if _sessions is None:
        _sessions = database()['sessions']
    return _sessions

_networks = None
def networks():
    global _networks
    if _networks is None:
        _networks = database()['networks']
    return _networks

_invitations = None
def invitations():
    global _invitations
    if _invitations is None:
        _invitations = database()['invitations']
    return _invitations

_transactions = None
def transactions():
    global _transactions
    if _transactions is None:
        _transactions = database()['transactions']
    return _transactions

# Collection that keep transaction history.
_finished_transactions = None
def finished_transactions():
    global _finished_transactions
    if _finished_transactions is None:
        _finished_transactions = database()['finished_transactions']
    return _finished_transactions

_notifications = None
def notifications():
    global _notifications
    if _notifications is None:
        _notifications = database()['notifications']
    return _notifications

_crashes = None
def crashes():
    global _crashes
    if _crashes is None:
        _crashes = database()['crashes']
    return _crashes

# functions
def byId(collection, _id):
    """
    Get an object from collection `collection' with its id `_id'
    """
    return collection.find_one({'_id': ObjectId(_id)})
