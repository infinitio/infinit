# -*- encoding: utf-8 -*-

import web
import hashlib
import json
import traceback
import urllib
import time
import re
import os
import pymongo

import error
from meta import conf
from meta import database
from meta import notifier
from meta import error
from meta import regexp

class Page(object):
    """
    Base class for all page, simplifies the use of viewers.
    It also wrap access (and cache) to session and users in a lazy load manner
    """
    __session__ = None # Set by the application.

    __application__ = None # Set by the application.

    __notifier = None

    _validators = []

    _mendatory_fields = []

    # These are set by the application when ran.
    mongo_host = None
    mongo_port = None

    # Lazy database attributes initialized on first access.
    _database_connection = None
    _database = None

    @property
    def database_connection(self):
        if self._database_connection is None:
            self._database_connection = database.Connection(
                self.mongo_host,
                self.mongo_port,
            )
        return self._database_connection

    @property
    def database(self):
        if self._database is None:
            self._database = database.Database(self.database_connection.meta)
        return self._database

    def __init__(self):
        self._input = None
        self._user = None
        if not web.ctx.host.startswith('v2.meta.api.') and \
           not web.ctx.host.startswith('localhost') and \
           not web.ctx.host.startswith('127.0.0.1') and \
           not web.ctx.host.startswith('192.168.') and \
           not 'development' in web.ctx.host:
            self.raise_error(error.DEPRECATED, msg = "Version not supported")

    @property
    def notifier(self):
        if self.__notifier is None:
            try:
                port = self.__application__.tropho_control_port
                self.__notifier = notifier.TrophoniusNotify(self.database)
                self.__notifier.open(("127.0.0.1", port))
            except Exception as e:
                print(e)
                return self.__notifier
        return self.__notifier

    @property
    def input(self):
        if self._input is None:
            self._input = web.input()
        return self._input

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

    #
    # User
    #
    @property
    def session(self):
        assert self.__session__ is not None
        return self.__session__

    @property
    def user(self):
        if self._user is None:
            try:
                self._user = self.database.users.find_one({
                    '_id': database.ObjectId(self.session._user_id)
                })
            except AttributeError:
                return None
        return self._user

    def logout(self):
        self.session.kill()

    def authenticate_with_token(self, token_genkey):
        user = self.database.users.find_one({
            'token_generation_key': token_genkey,
        })
        if not user:
            return False
        user.setdefault('connected', False)
        self.database.users.save(user)
        self.session._user_id = user['_id']
        self._user = user
        return True

    def authenticate(self, email, password):
        user = self.database.users.find_one({
            'email': email,
            'password': self.hashPassword(password)
        })
        if not user:
            return False
        user.setdefault('connected', False)
        if 'token_generation_key' not in user:
            tmp_gen_key = email + conf.SALT + str(time.time()) + conf.SALT
            user['token_generation_key'] = self.hashPassword(tmp_gen_key)
        self.database.users.save(user)
        self.session._user_id = user['_id']
        self._user = user
        return True

    def registerUser(self, **kwargs):
        kwargs['connected'] = False
        user = self.database.users.save(kwargs)
        return user

    def hashPassword(self, password):
        seasoned = password + conf.SALT
        seasoned = seasoned.encode('utf-8')
        return hashlib.md5(seasoned).hexdigest()

    #
    # Check and return values
    #
    def requireLoggedIn(self):
        if not self.user:
            self.forbidden("Authentication required.")

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

    def forbidden(self, msg):
        raise web.HTTPError("403 {}".format(msg))

    def error(self, error_code = error.UNKNOWN, msg = None, **kw):
        assert isinstance(error_code, tuple)
        assert len(error_code) == 2
        if msg is None:
            msg = error_code[1]
        assert isinstance(msg, str)
        res = {
            'success': False,
            'error_code': error_code[0],
            'error_details': msg,
        }
        res.update(kw)
        return json.dumps(res)

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
