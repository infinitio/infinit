# -*- encoding: utf-8 -*-

try:
    from PIL import Image
except ImportError:
    try:
        import Image
    except ImportError:
        print("Cannot import Image module, please install PIL")
        import sys
        sys.exit(1)

import StringIO
import json
import os
import random
import re
import sys
import time
import unicodedata
import web

from meta.page import Page
from meta import notifier
from meta import conf, database
from meta import error
from meta import regexp

import meta.mail
import meta.invitation

import metalib
import pythia

#
# Users
#

def user_by_id(db, _id, ensure_user_exists = True):
    assert isinstance(_id, database.ObjectId)
    user = db.users.find_one(_id)
    if ensure_user_exists:
        assert user is not None
    return user

def increase_swag(db, lhs, rhs, notifier_ = None):
    assert isinstance(lhs, database.ObjectId)
    assert isinstance(rhs, database.ObjectId)

    lh_user = user_by_id(db, lhs)
    rh_user = user_by_id(db, rhs)

    if lh_user is None or rh_user is None:
        raise Exception("unknown user")

    for user, peer in [(lh_user, rhs), (rh_user, lhs)]:
        user['swaggers'][str(peer)] = \
            user['swaggers'].setdefault(str(peer), 0) + 1;
        db.users.save(user)
        if user['swaggers'][str(peer)] == 1: # New swagger.
            if notifier_ is not None:
                notifier_.notify_some(
                    notifier.NEW_SWAGGER,
                    message = {'user_id': user['_id']},
                    recipient_ids = [peer,],
                )

class _Page(Page):
    def notify_swaggers(self, notification_id, data, user_id = None):
        if user_id is None:
            user = self.user
            user_id = user['_id']
        else:
            assert isinstance(user_id, database.ObjectId)
            user = self._user_by_id(user_id)

        swaggers = map(database.ObjectId, user['swaggers'].keys())
        d = {"user_id" : user_id}
        d.update(data)
        self.notifier.notify_some(
            notification_id,
            recipient_ids = swaggers,
            message = d,
        )
    def __ensure_user_exists(self, user):
        assert user is not None

    def _user_by_id(self, _id, ensure_user_exists = True):
        return user_by_id(self.database, _id, ensure_user_exists)

    def user_by_public_key(self, key, ensure_user_exists = True):
        user = self.database.users.find_one({'public_key': key})
        if ensure_user_exists:
            self.__ensure_user_exists(user)

        return user

    def user_by_email(self, email, ensure_user_exists = True):
        user = self.database.users.find_one({'email': email})
        if ensure_user_exists:
            self.__ensure_user_exists(user)

        return user

    def user_by_handle(self, handle, ensure_user_exists = True):
        user = self.database.users.find_one({'lw_handle': handle.lower()})
        if ensure_user_exists:
            self.__ensure_user_exists(user)

        return user

    def is_connected(self, user_id):
        assert isinstance(user_id, database.ObjectId)
        user = self.database.users.find_one(user_id)
        if not user:
            raise Exception("This user doesn't exist")
        connected = user.get('connected', False)
        assert isinstance(connected, bool)
        return connected

    def _increase_swag(self, lhs, rhs, notifier_ = None):
        increase_swag(self.database, lhs, rhs, notifier)

    def extract_user_fields(self, user):
        return {
            '_id': user['_id'],
            'public_key': user.get('public_key', ''),
            'fullname': user.get('fullname', ''),
            'handle': user.get('handle', ''),
            'connected_devices': user.get('connected_devices', []),
            'status': self.is_connected(user['_id']),
         }


class Search(Page):
    """ POST {
        text: 'query',
        offset: 0,
        limit: 5,
    }
    """
    __pattern__ = "/user/search"

    def POST(self):
        self.requireLoggedIn()

        text = self.data["text"]
        limit = int(self.data.get('limit', 5))
        offset = int(self.data.get('offset', 0))

        # While not sure it's an email or a fullname, search in both.
        users = []
        if not '@' in text:
            users = list(u['_id'] for u in self.database.users.find({
                    '$or' : [
                        {'fullname' : {'$regex' : '^%s' % text,  '$options': 'i'}},
                        {'handle' : {'$regex' : '^%s' % text, '$options': 'i'}},
                    ],
                    'register_status':'ok',
                },
                fields = ["_id"],
                limit = limit,
                skip = offset,
            ))

        return self.success({
            'users': users,
        })

class GenerateHandle(_Page):
    """ Generate handle from fullname
        GET /user/handle-for/(.+)
            -> fullname (plain/text)
    """
    __pattern__ = "/user/handle-for/(.+)"

    # No database check occurs there.
    def GET(self, fullname, enlarge = True):
        handle = ''
        for c in unicodedata.normalize('NFKD', fullname).encode('ascii', 'ignore'):
            if (c >= 'a'and c <= 'z') or (c >= 'A' and c <= 'Z') or c in '-_.':
                handle += c
            elif c in ' \t\r\n':
                handle += '.'

        if not enlarge:
            return handle

        if len(handle) < 5:
            handle += self._generate_dummy()
        return handle

    def _generate_dummy(self):
        t1 = ['lo', 'ca', 'ki', 'po', 'pe', 'bi', 'mer']
        t2 = ['ri', 'ze', 'te', 'sal', 'ju', 'il']
        t3 = ['yo', 'gri', 'ka', 'troll', 'man', 'et']
        t4 = ['olo', 'ard', 'fou', 'li']
        h = ''
        for t in [t1, t2, t3, t4]:
            h += t[int(random.random() * len(t))]
        return h

    def gen_unique(self, fullname):
        h = self.GET(fullname)
        while self.user_by_handle(h, ensure_user_exists = False):
            h += str(int(random.random() * 10))
        return h

class Message(Page):
    __pattern__ = "/debug"

    def POST(self):
        self.requireLoggedIn()
        self.notifier.notify_some(
            notifier.MESSAGE,
            recipient_ids = [database.ObjectId(self.data["recipient_id"]),],
            message = {
                'sender_id' : self.data['sender_id'],
                'message': self.data['message'],
            }
        )

        return self.success({})

class GetSwaggers(Page):
    __pattern__ =  "/user/swaggers"

    def GET(self):
        self.requireLoggedIn()
        return self.success({"swaggers" : self.user["swaggers"].keys()})

class AddSwagger(_Page):
    __pattern__ = "/user/add_swagger"

    def POST(self):
        print self.data['admin_token']

        if self.data['admin_token'] != pythia.constants.ADMIN_TOKEN:
            return self.error(error.UNKNOWN, "You're not admin")

        print(self.data['user1'], self.data['user2'])
        self._increase_swag(
            database.ObjectId(self.data['user1']),
            database.ObjectId(self.data['user2']),
            notifier_ = self.notifier
        )
        return self.success({"swag":"up"})

class Edit(Page):
    """ Edit fullname and handle.
    POST {
        'fullname': "New fullname",
        'handle': "New handle",
    }
    """
    __pattern__ = '/user/edit'

    def POST(self):
        self.requireLoggedIn()
        handle = GenerateHandle().GET(
            self.data['handle'].strip(),
            enlarge = False
        )
        lw_handle = handle.lower()
        fullname = self.data['fullname'].strip()
        if not len(fullname) > 4:
            return self.error(
                error.OPERATION_NOT_PERMITTED,
                "Fullname is too short"
            )
        other = self.database.users.find_one({'lw_handle': lw_handle})
        if other and other['_id'] != self.user['_id']:
            return self.error(error.HANDLE_ALREADY_REGISTRED)
        self.user['handle'] = handle
        self.user['lw_handle'] = lw_handle
        self.user['fullname'] = fullname
        self.database.users.save(self.user)
        return self.success()


class RemoveSwagger(Page):
    __pattern__ = "/user/remove_swagger"

    def POST(self):
        self.requireLoggedIn()
        swagez = self.database.users.find_and_modify(
            {"_id": database.ObjectId(self.user["_id"])},
            {"$pull": {"swaggers": self.data["_id"]}},
            True #upsert
        )
        return self.success({"swaggers" : swagez["swaggers"]})

class Invite(Page):
    # XXX doc and improve

    __pattern__ = "/user/invite"

    def POST(self):
        if self.data.get('admin_token') == pythia.constants.ADMIN_TOKEN:
            email = self.data['email'].strip()
            force = self.data.get('force', False)
            send_mail = not self.data.get('dont_send_email', False)
            if self.database.invitations.find_one({'email': email}):
                if not force:
                    return self.error(error.UNKNOWN, "Already invited!")
                else:
                    self.database.invitations.remove({'email': email})
            meta.invitation.invite_user(
                email,
                send_mail = send_mail,
                source = 'infinit',
                database = self.database
            )
            return self.success()
        else:
            self.requireLoggedIn()
            email = self.data['email'].strip()
            if self.user['remaining_invitations'] <= 0:
                return self.error(error.NO_MORE_INVITATION)
            self.user['remaining_invitations'] -= 1
            self.database.users.save(self.user)
            meta.invitation.invite_user(
                email,
                send_mail = True,
                source = self.user['email'],
                database = self.database
            )
            return self.success()

class Invited(Page):
    __pattern__ = "/user/invited"
    def GET(self):
        return self.success({
            'users': list(
                u['email'] for u in self.database.invitations.find(
                    {'source': self.user['email']},
                    fields = ['email'],
                )
            )
        })

class Favorite(Page):
    """Add a user to favorites
    POST {"user_id": other_user_id} -> {}
    """

    __pattern__ = "/user/favorite"

    def POST(self):
        self.requireLoggedIn()
        fav = database.ObjectId(self.data['user_id'])
        lst = self.user.setdefault('favorites', [])
        if fav not in lst:
            lst.append(fav)
            self.database.users.save(self.user)
        return self.success()

class Unfavorite(Page):
    """Remove a user from favorites
    POST {"user_id": other_user_id} -> {}
    """

    __pattern__ = "/user/unfavorite"

    def POST(self):
        self.requireLoggedIn()
        fav = database.ObjectId(self.data['user_id'])
        lst = self.user.setdefault('favorites', [])
        if fav in lst:
            lst.remove(fav)
            self.database.users.save(self.user)
        return self.success()


class Self(_Page):
    """
    Get self infos
        GET
            -> {
                'fullname': "My Name",
                'email': "My email",
                'handle': handle,
                'devices': [device_id1, ...],
                'networks': [network_id1, ...]
                'identity': 'identity string',
                'public_key': 'public_key string',
                'accounts': [
                    {'type':'account type', 'id':'unique account identifier'}
                ],
                'token_generation_key' : ...,
                'favorites': [user_id1, user_id2, ...],
            }
    """

    __pattern__ = "/self"

    def GET(self):
        self.requireLoggedIn()
        return self.success({
            '_id': self.user['_id'],
            'fullname': self.user['fullname'],
            'handle': self.user['handle'],
            'email': self.user['email'],
            'devices': self.user.get('devices', []),
            'networks': self.user.get('networks', []),
            'identity': self.user['identity'],
            'public_key': self.user['public_key'],
            'accounts': self.user['accounts'],
            'remaining_invitations': self.user.get('remaining_invitations', 0),
            'connected_devices': self.user.get('connected_devices', []),
            'status': self.is_connected(database.ObjectId(self.user['_id'])),
            'token_generation_key': self.user.get('token_generation_key', ''),
            'favorites': self.user.get('favorites', []),
            'created_at': self.user.get('created_at', 0),
        })

class Invitations(Page):
    """
    Get the remaining number of invitation.
        GET
            -> {
                   'remaining_invitations': 3,
               }
    """
    __pattern__ = "/user/remaining_invitations"

    def GET(self):
        self.requireLoggedIn()
        return self.success({
                'remaining_invitations': self.user.get('remaining_invitations', 0)
        })

class MinimumSelf(Page):
    """
    Get self infos
        GET
            -> {
                'email': "My email",
                'public_key': 'public_key string',
            }
    """

    __pattern__ = "/minimumself"

    def GET(self):
        self.requireLoggedIn() # scary
        return self.success({
            'email': self.user['email'],
            'identity': self.user['identity'],
        })

class One(_Page):
    """
    Get public informations of an user by id or email
        GET
            -> {
                '_id': "id",
                'email': "email",
                'public_key': "public key in base64",
            }
    """
    __pattern__ = "/user/(.+)/view"

    def GET(self, id_or_email):
        id_or_email = id_or_email.lower()
        if '@' in id_or_email:
            user = self.user_by_email(id_or_email, ensure_user_exists = False)
        else:
            user = self._user_by_id(database.ObjectId(id_or_email),
                              ensure_user_exists = False)
        if user is None:
            return self.error(error.UNKNOWN_USER)
        else:
            return self.success(self.extract_user_fields(user))

class FromPublicKey(_Page):
    __pattern__ = "/user/from_public_key"

    def POST(self):
        user = self.user_by_public_key(self.data['public_key'])
        return self.success(self.extract_user_fields(user))

class Avatar(_Page):
    """
        Get the an user's avatar.
        GET
            -> RAW_DATA (png 256x256)

        Update or create self image.
        POST <Image file>
            -> {"success": True}
    """

    __pattern__ = "/user/(.+)/avatar"

    def GET(self, _id):
        # Check if the user has any avatar
        user = self._user_by_id(database.ObjectId(_id), ensure_user_exists = False)
        image = user and user.get('avatar')
        if image:
            return str(image)
        else:
            # Otherwise return the default avatar
            with open(os.path.join(os.path.dirname(__file__), "place_holder_avatar.png"), 'rb') as f:
                return f.read(4096)

    def POST(self, _id):
        self.requireLoggedIn()
        try:
            raw_data = StringIO.StringIO(web.data())
            image = Image.open(raw_data)
            out = StringIO.StringIO()
            image.resize((256, 256)).save(out, 'PNG')
            out.seek(0)
        except:
            return self.error(msg = "Couldn't decode the image")
        self.user['avatar'] = database.Binary(out.read())
        self.database.users.save(self.user)
        return self.success()

class Register(_Page):
    """
    Register a new user
        POST {
            'email': "email@pif.net", #required
            'fullname': "The full name", #required
            'password': "password', #required
            'activation_code': dklds
        }
    """

    __pattern__ = "/user/register"

    _validators = [
        ('email', regexp.EmailValidator),
        ('fullname', regexp.HandleValidator),
        ('password', regexp.PasswordValidator),
    ]

    def POST(self):
        if self.user is not None:
            return self.error(error.ALREADY_LOGGED_IN)

        status = self.validate()
        if status:
            return self.error(status)

        user = self.data

        user['email'] = user['email'].lower()

        if len(user['activation_code']) == 8 and user['activation_code'] != "bitebite":
            user['activation_code'] = '@' + user['activation_code']

        source = None
        if self.database.users.find_one({
            'accounts': [{ 'type': 'email', 'id':user['email']}],
            'register_status': 'ok',
        }):
            return self.error(error.EMAIL_ALREADY_REGISTRED)
        elif user['activation_code'].startswith('@'):
            user['activation_code'] = user['activation_code'].upper()
            activation = self.database.activations.find_one({
                'code': user['activation_code']
            })
            if not activation or activation['number'] <= 0:
                return self.error(error.ACTIVATION_CODE_DOESNT_EXIST)
            ghost_email = user['email']
            source = user['activation_code']
        elif user['activation_code'] != 'bitebite':
            invitation = self.database.invitations.find_one({
                'code': user['activation_code'],
                'status': 'pending',
            })
            if not invitation:
                return self.error(error.ACTIVATION_CODE_DOESNT_EXIST)
            ghost_email = invitation['email']
            source = invitation['source']
            meta.invitation.move_from_invited_to_userbase(
                ghost_email,
                user['email']
            )
        else:
            ghost_email = user['email']

        ghost = self.database.users.find_one({
            'accounts': [{ 'type': 'email', 'id': ghost_email}],
            'register_status': 'ghost',
        })

        if ghost:
            user["_id"] = ghost['_id']
        else:
            user["_id"] = self.database.users.save({})

        identity, public_key = metalib.generate_identity(
            str(user["_id"]),
            user['email'], user['password'],
            conf.INFINIT_AUTHORITY_PATH,
            conf.INFINIT_AUTHORITY_PASSWORD
        )

        handle = GenerateHandle().gen_unique(user['fullname'])
        assert len(handle) >= 5

        user_id = self.registerUser(
            _id = user["_id"],
            register_status = 'ok',
            email = user['email'],
            fullname = user['fullname'],
            password = self.hashPassword(user['password']),
            identity = identity,
            public_key = public_key,
            handle = handle,
            lw_handle = handle.lower(),
            swaggers = ghost and ghost['swaggers'] or {},
            networks = ghost and ghost['networks'] or [],
            devices = [],
            connected_devices = [],
            notifications = ghost and ghost['notifications'] or [],
            old_notifications = [],
            accounts = [
                {'type':'email', 'id': user['email']}
            ],
            remaining_invitations = 3, #XXX
            status = False,
            created_at = time.time(),
        )
        if user['activation_code'].startswith('@'):
            self.database.activations.update(
                {"_id": activation["_id"]},
                {
                    '$inc': {'number': -1},
                    '$push': {'registered': user['email']},
                }
            )
        elif user['activation_code'] != 'bitebite':
            invitation['status'] = 'activated'
            self.database.invitations.save(invitation)

        self.notify_swaggers(
            notifier.NEW_SWAGGER,
            {
                'user_id' : str(user_id),
            },
            user_id = user_id,
        )

        return self.success({
            'registered_user_id': user['_id'],
            'invitation_source': source or '',
        })

class GenerateToken(Page):
    """
    Generate a token for further communication
        POST {
                 'token_generation_key': bla,
             }
             -> {
                     'success': True,
                     'token': "generated session token",
                     'fullname': 'full name',
                     'identity': 'Full base64 identity',
                     'handle': ...,
                     'email': ...,
                }
    """
    __pattern__ = "/user/generate_token"

    def POST(self):
        if self.user is not None:
            return self.error(error.ALREADY_LOGGED_IN)

        if self.authenticate_with_token(self.data['token_generation_key']):
            return self.success({
                "_id" : self.user["_id"],
                'token': self.session.session_id,
                'fullname': self.user['fullname'],
                'email': self.user['email'],
                'handle': self.user['handle'],
                'identity': self.user['identity'],
            })
        return self.error(error.ALREADY_LOGGED_IN)

class Login(Page):
    """
    Generate a token for further communication
        POST {
                  'email': "test@infinit.io",
                  'password': "hashed_password",
             }
             -> {
                     'success': True,
                     'token': "generated session token",
                     'fullname': 'full name',
                     'identity': 'Full base64 identity',
                     'handle': ...,
                     'email': ...,
                }
    """
    __pattern__ = "/user/login"

    _validators = [
        ('email', regexp.EmailValidator),
        ('password', regexp.PasswordValidator),
    ]

    def POST(self):
        if self.user is not None:
            return self.error(error.ALREADY_LOGGED_IN)

        status = self.validate()
        if status:
            return self.error(status)

        loggin_info = self.data
        loggin_info['email']= loggin_info['email'].lower()

        if self.authenticate(loggin_info['email'], loggin_info['password']):
            return self.success({
                "_id" : self.user["_id"],
                'token': self.session.session_id,
                'fullname': self.user['fullname'],
                'email': self.user['email'],
                'handle': self.user['handle'],
                'identity': self.user['identity'],
            })
        return self.error(error.EMAIL_PASSWORD_DONT_MATCH)

class _DeviceAccess(_Page):
    """
    Base class to update device connection status.
    """
    def __get_device(self, user_id, device_id):
        device = self.database.devices.find_one({
            '_id': device_id,
            'owner': user_id,
        })

        if device is None:
            self.raise_error(
                error.DEVICE_ID_NOT_VALID,
                "The device %s does not belong to the user %s" % (
                    device_id,
                    user_id
                )
            )
        return device

    def __set_connected(self, value, user_id, device_id):
        assert self._user_by_id(user_id)
        assert self.database.devices.find_one(device_id)

        device = self.__get_device(user_id, device_id)
        connected_before = self.is_connected(user_id)

        # Add / remove device from db
        req = {'_id': user_id}
        update_action = value and '$addToSet' or '$pull'
        self.database.users.update(
            req,
            {
                update_action: {'connected_devices': device['_id']},
            },
            multi = False,
        )
        user = self.database.users.find_one(user_id)

        # Disconnect only user with an empty list of connected device.
        req = {'_id': user_id}
        self.database.users.update(
            req,
            {"$set": {"connected": bool(user["connected_devices"])}},
            multi = False,
        )

        # XXX:
        # This should not be in user.py, but it's the only place
        # we know the device has been disconnected.
        if value is False:
            transactions = self.database.transactions.find(
                {
                    "nodes.%s" % str(device['_id']): {"$exists": True}
                }
            )

            for transaction in transactions:
                self.database.transactions.update(
                    {"_id": transaction},
                    {"$set": {"nodes.%s" % str(device_id): None}},
                    multi = False
                )
                self.notifier.notify_some(
                    notifier.PEER_CONNECTION_UPDATE,
                    device_ids = list(transaction['nodes'].keys()),
                    message =
                    {
                        "transaction_id": str(transaction['_id']),
                        "devices": list(transaction['nodes'].keys()),
                        "status": False
                    }
                )

        self.notify_swaggers(
            notifier.USER_STATUS,
            {
                'status': self.is_connected(user_id),
                'device_id': device_id,
                'device_status': value,
            },
            user_id = user_id,
        )

    def connect(self, user_id, device_id):
        self.__set_connected(True, user_id, device_id)

    def disconnect(self, user_id, device_id):
        self.__set_connected(False, user_id, device_id)

    action = None
    def POST(self):
        if self.data['admin_token'] != pythia.constants.ADMIN_TOKEN:
            return self.error(error.UNKNOWN, "You're not admin")
        self.action(
            database.ObjectId(self.data['user_id']),
            database.ObjectId(self.data['device_id']),
        )
        return self.success()

class Connect(_DeviceAccess):
    """
    Should only be called by Trophonius: add the given device to the list
    of connected devices. This means that notifications will be sent to that
    device.

    POST {
        "user_id": <user_id>,
        "device_id": <device_id>,
    }
    -> {
        'success': True,
    }
    """

    __pattern__ = "/user/connect"

    action = _DeviceAccess.connect

class Disconnect(_DeviceAccess):
    """
    Should only be called by Trophonius: remove the given device from the list
    of connected devices. This means that notification won't be sent to that
    device anymore.

    POST {
        'device_id': <device_id>,
        'user_id': <user_id>,
    }
    -> {
        'success': True,
    }
    """

    __pattern__ = "/user/disconnect"

    action = _DeviceAccess.disconnect

class Logout(Page):
    """
    GET
        -> {
            'success': True
        }
    """

    __pattern__ = "/user/logout"

    def GET(self):
        if not self.user:
            return self.error(error.NOT_LOGGED_IN)
        self.logout()
        return self.success()
