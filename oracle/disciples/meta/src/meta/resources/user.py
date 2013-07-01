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

import json
import os
import random
import re
import StringIO
import unicodedata
import web
import sys

from meta.page import Page
from meta import notifier
from meta import conf, database
from meta import error
from meta import regexp

import meta.mail
import meta.invitation

import metalib
import pythia

class Search(Page):
    __pattern__ = "/user/search"

    def POST(self):
        self.requireLoggedIn()

        text = self.data["text"]
        count = 'count' in self.data and self.data['count'] or 5
        offset = 'offset' in self.data and self.data['offset'] or 0

        # While not sure it's an email or a fullname, search in both.
        users = []
        if not '@' in text:
            users = database.users().find(
                {
                    '$or' : [
                        {'fullname' : {'$regex' : '^%s' % text,  '$options': 'i'}},
                        {'handle' : {'$regex' : '^%s' % text, '$options': 'i'}},
                    ],
                    'register_status':'ok',
                },
                fields=["_id"],
                limit=count + offset
            )
        # else:
        #     users = database.users().find(
        #         {'email' : {'$regex' : '^%s' %text, '$options': 'i'}},
        #         fields=["_id"],
        #         limit=count + offset
        #     )

        result = list(user['_id'] for user in users[offset:count])

        return self.success({
            'users': result,
        })

class GenerateHandle(Page):
    """ Generate handle from fullname
        GET /user/handle-for/(.+)
            -> fullname (plain/text)
    """
    __pattern__ = "/user/handle-for/(.+)"

    # No database check occurs there.
    def GET(self, fullname):
        handle = ''
        for c in unicodedata.normalize('NFKD', fullname).encode('ascii', 'ignore'):
            if (c >= 'a'and c <= 'z') or (c >= 'A' and c <= 'Z') or c in '-_.':
                handle += c
            elif c in ' \t\r\n':
                handle += '.'

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
        while database.users().find_one({'handle': h}):
            h += str(int(random.random() * 10))
        return h

class Message(Page):
    __pattern__ = "/debug"

    def POST(self):
        self.requireLoggedIn()
        self.notifier.notify_some(
            notifier.MESSAGE,
            recipient_ids = [database.ObjectId(self.data["recipient_id"]),],
            {
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

class AddSwagger(Page):
    __pattern__ = "/user/add_swagger"

    _validators = [
        ('email', regexp.EmailValidator),
        ('fullname', regexp.HandleValidator),
    ]

    def POST(self):
        self.requireLoggedIn()
        try:
            if "email" in self.data:
                error_code = _validators['email'](self.data['email'])
                if error_code:
                    return self.error(error_code)
                user = database.users().find_and_modify(
                    {"_id": pymongo.objectid.ObjectId(self.user["_id"])},
                    {"$addToSet": {"pending_swaggers": self.data["email"]}},
                )
                # XXX check for already exists.
                return self.success({"what" : "pending"})
            elif "_id" in self.data and \
                    self.user["_id"] != self.data["_id"]:
                swagger = database.byId(database.users(), self.data["_id"])
                if swagger:
                    user = database.users().find_and_modify(
                        {"_id" : pymongo.objectid.ObjectId(self.user["_id"])},
                        {"$addToSet" : {"swaggers" : self.data["_id"]}},
                    )
                else:
                    return self.error(error.UNKNOWN, "don't try to fake the swag")
        except Exception as e:
            return self.error(error.UNKNOWN, "your swag is too low !")
        return self.success({"swag":"up"})

class RemoveSwagger(Page):
    __pattern__ = "/user/remove_swagger"

    def POST(self):
        self.requireLoggedIn()
        swagez = database.users().find_and_modify(
            {"_id": database.ObjectId(self.user["_id"])},
            {"$pull": {"swaggers": self.data["_id"]}},
            True #upsert
        )
        return self.success({"swaggers" : swagez["swaggers"]})

class Invite(Page):
    # XXX doc and improve

    __pattern__ = "/user/invite"

    def POST(self):
        if self.data['admin_token'] != pythia.constants.ADMIN_TOKEN:
            return self.error(error.UNKNOWN, "You're not admin")
        email = self.data['email'].strip()
        force = self.data.get('force', False)
        send_mail = not self.data.get('dont_send_email', False)
        if database.invitations().find_one({'email': email}):
            if not force:
                return self.error(error.UNKNOWN, "Already invited!")
            else:
                database.invitations().remove({'email': email})
        meta.invitation.invite_user(email, send_mail = send_mail)
        return self.success()


class Self(Page):
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
                ]
                'token_generation_key' : ...
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
            'status': self.user.get('connected', False),
            'token_generation_key': self.user.get('token_generation_key', ''),
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

def extract_user_fields(user):
    return {
        '_id': user['_id'],
        'public_key': user.get('public_key', ''),
        'fullname': user.get('fullname', ''),
        'handle': user.get('handle', ''),
        'connected_devices': user.get('connected_devices', []),
        'status': user['connected'] ,
     }

class One(Page):
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
            user = database.users().find_one({'email': id_or_email})
        else:
            user = database.byId(database.users(), id_or_email)
        if not user:
            return self.error(error.UNKNOWN_USER, "Couldn't find user for id '%s'" % str(id_or_email))
        return self.success(extract_user_fields(user))

class FromPublicKey(Page):
    __pattern__ = "/user/from_public_key"

    def POST(self):
        user = database.users().find_one({
            'public_key': self.data['public_key'],
        })

        if not user:
            return self.error(error.UNKNOWN, "No user could be found for this public key!")
        return self.success(extract_user_fields(user))

class Avatar(Page):
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
        user = database.users().find_one(database.ObjectId(_id))
        image = user and user.get('avatar')
        if image:
            yield str(image)

        with open(os.path.join(os.path.dirname(__file__), "place_holder_avatar.png"), 'rb') as f:
            while 1:
                data = f.read(4096)
                if data:
                    yield data
                else:
                    break


    def POST(self, _id):
        self.requireLoggedIn()
        raw_data = StringIO.StringIO(web.data())
        image = Image.open(raw_data)
        out = StringIO.StringIO()
        image.resize((256, 256)).save(out, 'PNG')
        out.seek(0)
        self.user['avatar'] = database.Binary(out.read())
        database.users().save(self.user)
        return self.success()

class Register(Page):
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

        if database.users().find_one({
            'accounts': [{ 'type': 'email', 'id':user['email']}],
            'register_status': 'ok',
        }):
            return self.error(error.EMAIL_ALREADY_REGISTRED)

        elif user['activation_code'] != 'bitebite': # XXX
            invitation = database.invitations().find_one({
                'code': user['activation_code'],
                'status': 'pending',
            })
            if not invitation:
                return self.error(error.ACTIVATION_CODE_DOESNT_EXIST)
            ghost_email = invitation['email']
            meta.invitation.move_from_invited_to_userbase(ghost_email, user['email'])
        else:
            ghost_email = user['email']

        ghost = database.users().find_one({
            'accounts': [{ 'type': 'email', 'id':ghost_email}],
            'register_status': 'ghost',
            'connected': False,
        })

        if ghost:
            user["_id"] = ghost['_id']
        else:
            user["_id"] = database.users().save({})

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
        )
        if user['activation_code'] != 'bitebite': #XXX
            invitation['status'] = 'activated'
            database.invitations().save(invitation)
        return self.success()

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



class _DeviceAccess(Page):
    """
    Base class to update device connection status.
    """
    def __get_device(self, user_id, device_id):
        device = database.devices().find_one({
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

    def is_connected(self, user_id):
        res = database.users().find_one(user_id)['connected']
        assert isinstance(res, bool)
        return res

    def __set_connected(self, value, user_id, device_id):
        assert database.users().find_one(user_id)
        assert database.devices().find_one(device_id)

        device = self.__get_device(user_id, device_id)
        connected_before = self.is_connected(user_id)

        # Add / remove device from db
        req = {'_id': user_id}
        update_action = value and '$push' or '$pull'
        database.users().update(
            req,
            {
                update_action: {'connected_devices': device['_id']},
            },
            multi = False,
        )

        # Disconnect only user with an empty list of connected device.
        if value is False:
            req['connected_devices'] = []
        req = {'_id': user_id}
        database.users().update(
            req,
            {"$set": {"connected": value}},
            multi = False,
        )

        self.notifySwaggers(
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
