# -*- encoding: utf-8 -*-

from meta.page import Page, requireLoggedIn
from meta import database, conf
from meta import error
from meta import regexp

import metalib

class _Page(Page):
    """Common tools for user calls."""

    # Smart getter for user in database.
    def user_check(self, user):
        if user is None:
            return self.forbidden("Couldn't find any user with this id")
        return user

    @requireLoggedIn
    def user_by_handle(self, handle):
        req = {
            'lw_handle': handle.lower(),
        }
        return self.user_check(database.users().find_one(req))

#    @requireLoggedIn
    def user_by_K(self, K):
        req = {
            'K': K,
        }
        return self.user_check(database.users().find_one(req))

class AskChallenge(_Page):
    """ doc goes here """

    __mandatory_fields__ = [
        ('K', basestring),
    ]

    __pattern__ = "/identity/ask_challenge"

    def POST(self):
        if self.user is not None:
            return self.error(error.ALREADY_LOGGED_IN)

        user = self.user_by_K(self.data['K'])

        challenge, nonce = metalib.generate_challenge(user['K'])

        user.setdefault('challenges', dict())[challenge] = nonce
        database.users().save(user)

        return self.success({"challenge": challenge})

class AnswerChallenge(_Page):
    """ doc goes here """

    __mandatory_fields__ = [
        ('K', basestring),
        ('challenge', basestring),
        ('response', basestring),
    ]

    __pattern__ = "/identity/answer_challenge"

    def POST(self):
        if self.user is not None:
            return self.error(error.ALREADY_LOGGED_IN)

        user = self.user_by_K(self.data['K'])

        challenge = self.data['challenge']
        if challenge not in user.setdefault('challenges', dict()).keys():
            return self.error(error.UNKNOWN)

        res = metalib.verify_challenge(self.data['response'],
                                       user['challenges'][challenge])

        if res is None or res == False:
            return self.error(error.UNKNOWN)

        user['challenges'].pop(challenge)
        database.users().save(user)

        return self.success({'token_generation_key': user['token_generation_key']})

class Signin(_Page):
    """ doc goes here """

    __pattern__ = "/identity/signin"

    def POST(self):
        K = self.data['public_key']
        handle = self.data['handle'].strip()
        lw_handle = handle.lower()

        user = database.users().find_one({'$or': [{'K': K}, {'lw_handle': lw_handle}]})
        if user is not None:
            if lw_handle == user['lw_handle']:
                return self.error(error.HANDLE_ALREADY_REGISTRED)
            else:
                return self.error(error.UNKNOWN, "K already registred")

        import time
        req = {
            "K": K,
            "handle": handle,
            "lw_handle": lw_handle,
            "token_generation_key": self.hashPassword(handle +
                                                      conf.SALT +
                                                      str(time.time()) +
                                                      conf.SALT)
        }

        database.users().save(req)

        return self.success({})

class Signout(_Page):
    """ doc goes here """

    __pattern__ = "/identity/signout"

    @requireLoggedIn
    def GET(self):
        # If we store the networks into the user, that allows use to do only:
        # database.users().remove(self.user)
        # For the moment, we must remove all the networks the user own.

        for network_name in self.get("owned_networks", []):
            database.networks().remove({"name": network_name, "owner": self['_id']})
        database.users().remove(self.user)

        return self.success({})
