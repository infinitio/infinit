# -*- encoding: utf-8 -*-

import json
import web

from meta.page import Page
from meta import notifier
from meta import conf, database
from meta import error
from meta import regexp

import meta.mail

import os
import re

import metalib
import pythia

class Search(Page):
    __pattern__ = "/user/search"

    def POST(self):
        text = self.data["text"]
        count = 'count' in self.data and self.data['count'] or 5
        offset = 'offset' in self.data and self.data['offset'] or 0

        # While not sure it's an email or a fullname, search in both.
        if not '@' in text:
            users = database.users().find(
                {
                    '$or' : [
                        {'fullname' : {'$regex' : '^%s' % text,  '$options': 'i'}},
                        {'email' : {'$regex' : '^%s' % text, '$options': 'i'}},
                    ]
                },
                fields=["_id"],
                limit=count + offset
            )
        else:
            users = database.users().find(
                {'email' : {'$regex' : '^%s' %text, '$options': 'i'}},
                fields=["_id"],
                limit=count + offset
            )

        result = list(user['_id'] for user in users[offset:])

        return self.success({
            'users': result,
        })

class Message(Page):
    __pattern__ = "/debug"

    def POST(self):
        self.notifier.notify_one(
            notifier.MESSAGE,
            self.data["recipient_id"],
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
        import pymongo

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
        import pymongo

        swagez = database.users().find_and_modify(
            {"_id": pymongo.objectid.ObjectId(self.user["_id"])},
            {"$pull": {"swaggers": self.data["_id"]}},
            True #upsert
        )
        return self.success({"swaggers" : swagez["swaggers"]})

class FromPublicKey(Page):
    __pattern__ = "/user/from_public_key"

    def POST(self):
        user = database.users().find_one({
            'public_key': self.data['public_key'],
        })

        if not user:
            return self.error(error.UNKNOWN, "No user could be found for this public key!")
        return self.success({
            '_id': user['_id'],
            'email': user['email'],
            'public_key': user['public_key'],
            'fullname': user['fullname'],
        })

class Invite(Page):
    # XXX doc and improve

    __pattern__ = "/user/invite"

    def POST(self):
        if self.data['admin_token'] != pythia.constants.ADMIN_TOKEN:
            return self.error(error.UNKNOWN, "You're not admin")
        email = self.data['email'].strip()
        force = self.data.get('force', False)
        send_email = not self.data.get('dont_send_email', False)
        if database.invitations().find_one({'email': email}):
            if not force:
                return self.error(error.UNKNOWN, "Already invited!")
            else:
                database.invitations().remove({'email': email})
        code = self._generate_code(email)
        content = INVITATION_CONTENT % {
            'activation_code': code,
            'space': ' ',
        }
        if send_email:
            meta.mail.send(email, INVITATION_SUBJECT, content)
        database.invitations().insert({
            'email': email,
            'status': 'pending',
            'code': code,
        })
        return self.success()

    def _generate_code(self, mail):
        import hashlib
        import time
        hash_ = hashlib.md5()
        hash_.update(mail.encode('utf8') + str(time.time()))
        return hash_.hexdigest()


class Self(Page):
    """
    Get self infos
        GET
            -> {
                'fullname': "My Name",
                'email': "My email",
                'devices': [device_id1, ...],
                'networks': [network_id1, ...]
                'identity': 'identity string',
                'public_key': 'public_key string',
                'accounts': [
                    {'type':'account type', 'id':'unique account identifier'}
                ]
            }
    """

    __pattern__ = "/self"

    def GET(self):
        if not self.user:
            return self.error(error.NOT_LOGGED_IN)
        return self.success({
            '_id': self.user['_id'],
            'fullname': self.user['fullname'],
            'email': self.user['email'],
            'devices': self.user.get('devices', []),
            'networks': self.user.get('networks', []),
            'identity': self.user['identity'],
            'public_key': self.user['public_key'],
            'accounts': self.user['accounts'],
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
        if not self.user:
            return self.error(error.NOT_LOGGED_IN)
        return self.success({
            'email': self.user['email'],
            'identity': self.user['identity'],
        })

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
        if '@' in id_or_email:
            user = database.users().find_one({'email': id_or_email})
        else:
            user = database.byId(database.users(), id_or_email)
        if not user:
            return self.error(error.UNKNOWN_USER, "Couldn't find user for id '%s'" % str(id_or_email))
        return self.success({
            '_id': user['_id'],
            'email': user['email'],
            'public_key': user['public_key'],
            'fullname': user['fullname'],
            # XXX: user['connected']
            'status': 1, #user['status']
        })

class Icon(Page):
    """
        Get the icon of an user.
        GET
            -> RAW_DATA (png 256x256)
    """

    __pattern__ = "/user/(.+)/icon"

    def GET(self, _id):
        if not self.user or _id not in self.user.swaggers:
            raise web.forbidden()
        with open(os.path.join(os.path.dirname(__file__), "pif.png"), 'rb') as f:
            while 1:
                data = f.read(4096)
                if data:
                    yield data
                else:
                    break

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
            return self.error(*status)

        user = self.data

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
                return self.error(error.ACTIVATION_CODE_DOESNT_EXIST, "'%s' is not a valid activation code." % user['activation_code'])

        ghost = database.users().find_one({
            'accounts': [{ 'type': 'email', 'id':user['email']}],
            'register_status': 'ghost',
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

        user_id = self.registerUser(
            _id = user["_id"],
            register_status = 'ok',
            email = user['email'],
            fullname = user['fullname'],
            password = self.hashPassword(user['password']),
            identity = identity,
            public_key = public_key,
            swaggers = {},
            networks = [],
            devices = [],
            notifications = (ghost and ghost['notifications'] or []),
            old_notifications = [],
            accounts = [
                {'type':'email', 'id': user['email']}
            ]
        )
        if user['activation_code'] != 'bitebite': #XXX
            invitation['status'] = 'activated'
            database.invitations().save(invitation)
        return self.success()

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
            return self.error(*status)

        loggin_info = self.data

        if self.authenticate(loggin_info['email'], loggin_info['password']):
            return self.success({
                "_id" : self.user["_id"],
                'token': self.session.session_id,
                'fullname': self.user['fullname'],
                'email': self.user['email'],
                'identity': self.user['identity'],
            })
        return self.error(error.EMAIL_PASSWORD_DONT_MATCH)

class Disconnection(Page):
    """
    POST {
              'user_id': "the user id".
              'user_token': 'the user token'
              'full': no more device for this client connected.
         }
         -> {
                 'success': True
            }
    """

    __pattern__ = "/user/disconnected"

    _validators = [
        ('user_id', regexp.UserIDValidator),
    ]

    def POST(self):
        if self.data['admin_token'] != pythia.constants.ADMIN_TOKEN:
            return self.error(error.UNKNOWN, "You're not admin")

        if not self._user:
            return self.error(error.UNKNOWN)

        _id = self.data['user_id']
        token = self.data['user_token']

        del self.session.store[token]

        self._user['connected'] = bool(self.data['full'])

        database.users().save(self._user)

        return self.success()

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
        self.notifySwaggers(
            notifier.USER_STATUS,
            {
                "status" : 2,
            }
        )
        return self.success()
