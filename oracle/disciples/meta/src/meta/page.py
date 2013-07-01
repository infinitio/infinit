# -*- encoding: utf-8 -*-

import web
import hashlib
import json
import traceback
import urllib
import time
import re
import os

import error
from meta import conf
from meta import database
from meta import notifier
from meta import error
from meta import regexp
from meta import apertus

class Page(object):
    """
    Base class for all page, simplifies the use of viewers.
    It also wrap access (and cache) to session and users in a lazy load manner
    """
    __session__ = None #set by the application

    __notifier = None

    __apertus = None

    _validators = []

    _mendatory_fields = []

    def __init__(self):
        self._input = None
        self._user = None

    @property
    def session(self):
        assert self.__session__ is not None
        return self.__session__

    @property
    def user(self):
        if self._user is None:
            try:
                self._user = database.users().find_one({
                    '_id': database.ObjectId(self.session._user_id)
                })
            except AttributeError:
                return None
        return self._user

    @property
    def notifier(self):
        if self.__notifier is None:
            try:
                self.__notifier = notifier.TrophoniusNotify()
                self.__notifier.open()
            except Exception as e:
                print(e)
                return self.__notifier
        return self.__notifier


    @property
    def apertus(self):
        if self.__apertus is None:
            self.__apertus = apertus.Apertus()
            return self.__apertus
        return self.__apertus

    @property
    def input(self):
        if self._input is None:
            self._input = web.input()
        return self._input

    def validate(self):
        for (field, validator) in self._validators:
            if not field in self.data.keys():
                return (error.BAD_REQUEST[0], "Field %s is mandatory" % field)
            else:
                error_code = validator(self.data[field])
                if error_code:
                    return error_code
        for (field, type_) in self._mendatory_fields:
            if not field in self.data.keys() or not isinstance(self.data[field], type_):
                return (error.BAD_REQUEST[0], "Field %s is mandatory and must be an %s" % (field, type_))
        return ()

    def logout(self):
        self.session.kill()

    def authenticate_with_token(self, token_genkey):
        user = database.users().find_one({
            'token_generation_key': token_genkey,
        })
        if not user:
            return False
        user.setdefault('connected', False)
        database.users().save(user)
        self.session._user_id = user['_id']
        self._user = user
        return True

    def authenticate(self, email, password):
        user = database.users().find_one({
            'email': email,
            'password': self.hashPassword(password)
        })
        if not user:
            return False
        user.setdefault('connected', False)
        if 'token_generation_key' not in user:
            tmp_gen_key = email + conf.SALT + str(time.time()) + conf.SALT
            user['token_generation_key'] = self.hashPassword(tmp_gen_key)
        database.users().save(user)
        self.session._user_id = user['_id']
        self._user = user
        return True

    def registerUser(self, **kwargs):
        kwargs['connected'] = False
        user = database.users().save(kwargs)
        return user

    @staticmethod
    def connected(user_id):
        assert isinstance(user_id, database.ObjectId)
        user = database.users().find_one(user_id)
        if not user:
            raise Exception("This user doesn't exist")
        return user.get('connected', False)

    def forbidden(self, msg):
        raise web.HTTPError("403 {}".format(msg))

    def requireLoggedIn(self):
        if not self.user:
            self.forbidden("Authentication required.")

    def hashPassword(self, password):
        seasoned = password + conf.SALT
        seasoned = seasoned.encode('utf-8')
        return hashlib.md5(seasoned).hexdigest()

    def notifySwaggers(self, notification_id, data, user_id = None):
        if user_id is None:
            user = self.user
            user_id = user['_id']
        else:
            assert isinstance(user_id, database.ObjectId)
            user = database.users().find_one(user_id)

        swaggers = list(
            swagger_id for swagger_id in user['swaggers'].keys()
            if self.connected(database.ObjectId(swagger_id))
        )
        d = {"user_id" : user_id}
        d.update(data)
        self.notifier.notify_some(
            notification_id,
            recipient_ids = swaggers,
            message = d,
            store = False,
        )

    def error(self, error_code = error.UNKNOWN, msg = None):
        assert isinstance(error_code, tuple)
        assert len(error_code) == 2
        if msg is None:
            msg = error_code[1]
        assert isinstance(msg, str)
        return json.dumps({
            'success': False,
            'error_code': error_code[0],
            'error_details': msg,
        })

    def raise_error(self, error_code, msg = None):
        raise web.ok(data = self.error(error_code, msg))

    def success(self, obj={}):
        assert(isinstance(obj, dict))
        d = {'success': True}
        d.update(obj)
        res = json.dumps(d, default=str)
        web.header('Content-Type', 'application/json')
        web.header('Content-Length', str(len(res)))
        return res

    _data = None
    @property
    def data(self):
        if self._data is None:
            try:
                data = web.data()
                if web.ctx.env['CONTENT_TYPE'] != 'application/json':
                    data = urllib.unquote(data)
                self._data = json.loads(data)
            except:
                traceback.print_exc()
                print "Cannot decode", data, web.data()
                raise ValueError("Wrong post data")
        return self._data
