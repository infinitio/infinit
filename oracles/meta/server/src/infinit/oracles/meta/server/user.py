# -*- encoding: utf-8 -*-

import bottle
import bson
import uuid
import elle.log

from .utils import api, require_logged_in, require_admin, hash_pasword
from . import error, notifier, regexp, conf, invitation

from pymongo import DESCENDING
import os
import string
import time
import unicodedata

#
# Users
#
ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.User'

class Mixin:

  ## ------ ##
  ## Handle ##
  ## ------ ##
  def generate_dummy(self):
    import random
    t1 = ['lo', 'ca', 'ki', 'po', 'pe', 'bi', 'mer']
    t2 = ['ri', 'ze', 'te', 'sal', 'ju', 'il']
    t3 = ['yo', 'gri', 'ka', 'tro', 'man', 'et']
    t4 = ['olo', 'ard', 'fou', 'li']
    h = ''
    for t in [t1, t2, t3, t4]:
      h += t[int(random.random() * len(t))]
    return h

  def __generate_handle(self,
                        fullname,
                        enlarge = True):
    assert isinstance(fullname, str)
    with elle.log.trace("generate handle from fullname %s" % fullname):
      allowed_characters = string.ascii_letters + string.digits
      allowed_characters += '_'
      normalized_name = unicodedata.normalize('NFKD', fullname.strip().replace(' ', '_'))
      handle = ''
      for c in normalized_name:
        if c in allowed_characters:
          handle += c

      elle.log.debug("clean handle: %s" % handle)

      if len(handle) > 30:
        handle = handle[:30]

      if not enlarge:
        return handle

      if len(handle) < 5:
        handle += self.generate_dummy()
        elle.log.debug("enlarged handle: %s" % handle)

      return handle

  def generate_handle(self,
                      fullname):
    """ Generate handle from a given fullname.

    fullname -- plain text user fullname.
    """
    return self.__generate_handle(fullname)

  def unique_handle(self,
                    fullname):
    import random
    h = self.__generate_handle(fullname)
    while self.user_by_handle(h, ensure_existence = False):
      h += str(int(random.random() * 10))
    return h

  ## -------- ##
  ## Sessions ##
  ## -------- ##
  @api('/login', method = 'POST')
  def login(self,
            email,
            password,
            device_id: uuid.UUID):
    with elle.log.trace("%s: log on device %s" % (email, device_id)):
      assert isinstance(device_id, uuid.UUID)
      email = email.lower()
      user = self.database.users.find_one({
        'email': email,
        'password': hash_pasword(password),
      })
      if user is None:
        self.fail(error.EMAIL_PASSWORD_DONT_MATCH)
      query = {'id': str(device_id), 'owner': user['_id']}
      elle.log.debug("%s: look for session" % email)
      device = self.device(ensure_existence = False, **query)
      if device is None:
        elle.log.trace("user logged with an unknow device")
        device = self._create_device(id = device_id,
                                     owner = user)
      else:
        assert str(device_id) in user['devices']

      # Remove potential leaked previous session.
      self.sessions.remove({'email': email, 'device': device['_id']})
      elle.log.debug("%s: store session" % email)
      bottle.request.session['device'] = device['_id']
      bottle.request.session['email'] = email

      user = self.user
      elle.log.trace("%s: successfully connected as %s on device %s" %
                     (email, user['_id'], device['id']))

      return self.success(
        {
          '_id': user['_id'],
          'fullname': user['fullname'],
          'email': user['email'],
          'handle': user['handle'],
          'identity': user['identity'],
          'device_id': device['id'],
        }
    )

  @api('/web-login', method = 'POST')
  def web_login(self,
                email,
                password):
    with elle.log.trace("%s: web login" % email):
      email = email.lower()
      user = self.database.users.find_one({
        'email': email,
        'password': hash_pasword(password),
      })
      if user is None:
        self.fail(error.EMAIL_PASSWORD_DONT_MATCH)

      elle.log.debug("%s: store session" % email)
      bottle.request.session['email'] = email
      user = self.user

      elle.log.trace("%s: successfully connected as %s" %
                     (email, user['_id']))

      return self.success(
        {
          '_id' : user['_id'],
          'fullname': user['fullname'],
          'email': user['email'],
          'handle': user['handle'],
        }
      )

  @api('/logout', method = 'POST')
  @require_logged_in
  def logout(self):
    user = self.user
    with elle.log.trace("%s: logout" % user['email']):
      if 'email' in bottle.request.session:
        elle.log.debug("%s: remove session" % user['email'])
        # Web sessions have no device.
        if 'device' in bottle.request.session:
          elle.log.debug("%s: remove session device" % user['email'])
          del bottle.request.session['device']
        del bottle.request.session['email']
        return self.success()
      else:
        return self.fail(error.NOT_LOGGED_IN)

  @property
  def user(self):
    elle.log.trace("get user from session")
    email = bottle.request.session.get('email', None)
    if email is not None:
      return self.user_by_email(email)
    elle.log.trace("session not found")

  ## -------- ##
  ## Register ##
  ## -------- ##
  def _register(self, **kwargs):
    kwargs['connected'] = False
    user = self.database.users.save(kwargs)
    return user

  @api('/user/register', method = 'POST')
  def register(self,
               email,
               password,
               fullname,
               activation_code = None):
    """Register a new user.

    email -- the account email.
    password -- the client side hashed password.
    fullname -- the user fullname.
    activation_code -- the activation code.
    """
    _validators = [
      (email, regexp.EmailValidator),
      (password, regexp.PasswordValidator),
      (fullname, regexp.FullnameValidator),
    ]

    for arg, validator in _validators:
      res = validator(arg)
      if res != 0:
        return self.fail(res)

    fullname = fullname.strip()

    with elle.log.trace("registration: %s as %s" % (email, fullname)):
      if self.user is not None:
        return self.fail(error.ALREADY_LOGGED_IN)
      email = email.strip().lower()

      source = None
      if self.database.users.find_one(
        {
          'accounts': [{ 'type': 'email', 'id': email}],
          'register_status': 'ok',
        }):
        return self.fail(error.EMAIL_ALREADY_REGISTRED)

      ghost = self.database.users.find_one(
        {
          'accounts': [{ 'type': 'email', 'id': email}],
          'register_status': 'ghost',
        })

      if ghost is not None:
        id = ghost['_id']
      else:
        id = self.database.users.save({})

      elle.log.trace('id: %s' % id)

      import papier
      with elle.log.trace('generate identity'):
        identity, public_key = papier.generate_identity(
          str(id),
          email,
          password,
          conf.INFINIT_AUTHORITY_PATH,
          conf.INFINIT_AUTHORITY_PASSWORD
          )

        handle = self.unique_handle(fullname)

        user_id = self._register(
          _id = id,
          register_status = 'ok',
          email = email,
          fullname = fullname,
          password = hash_pasword(password),
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
            {'type':'email', 'id': email}
          ],
          status = False,
          created_at = time.time(),
        )

        with elle.log.trace("add user to the mailing list"):
          self.invitation.subscribe(email)

        assert user_id == id

        self._notify_swaggers(
          notifier.NEW_SWAGGER,
          {
            'user_id' : str(user_id),
          },
          user_id = user_id,
        )

        return self.success({
          'registered_user_id': user_id,
          'invitation_source': source or '',
        })

  @api('/user/<id>/connected')
  def is_connected(self, id: bson.ObjectId):
    try:
      return self.success({"connected": self._is_connected(id)})
    except error.Error as e:
      self.fail(*e.args)

  ## -------------- ##
  ## Search helpers ##
  ## -------------- ##
  def __ensure_user_existence(self, user):
    """Raise if the given user is not valid.

    user -- the user to validate.
    """
    if user is None:
      raise Exception("user doesn't exist")

  def _user_by_id(self, _id, ensure_existence = True):
    """Get a user using by id.

    _id -- the _id of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    assert isinstance(_id, bson.ObjectId)
    user = self.database.users.find_one(_id)
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_public_key(self, key, ensure_existence = True):
    """Get a user from is public_key.

    public_key -- the public_key of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    user = self.database.users.find_one({'public_key': key})
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_email(self, email, ensure_existence = True):
    """Get a user from is email.

    email -- the email of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    user = self.database.users.find_one({'email': email})
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  def user_by_handle(self, handle, ensure_existence = True):
    """Get a user from is handle.

    handle -- the handle of the user.
    ensure_existence -- if set, raise if user is invald.
    """
    user = self.database.users.find_one({'lw_handle': handle.lower()})
    if ensure_existence:
      self.__ensure_user_existence(user)
    return user

  ## ------ ##
  ## Search ##
  ## ------ ##

  @property
  def user_public_fields(self):
    res = {
      'id': '$_id',
      'public_key': '$public_key',
      'fullname': '$fullname',
      'handle': '$handle',
      'connected_devices': '$connected_devices',
      'status': '$status',
      '_id': False,
    }
    if self.admin:
      res['email'] = '$email'
    return res

  @api('/users')
  def users(self, search = None, limit : int = 5, skip : int = 0):
    """Search the ids of the users with handle or fullname matching text.

    search -- the query.
    skip -- the number of user to skip in the result (optional).
    limit -- the maximum number of match to return (optional).
    """
    with elle.log.trace('search %s (limit: %s, skip: %s)' % \
                        (search, limit, skip)):
      pipeline = []
      match = {
        'register_status':'ok',
      }
      if search is not None:
        match['$or'] = [
          {'fullname' : {'$regex' : search,  '$options': 'i'}},
          {'handle' : {'$regex' : search, '$options': 'i'}},
        ]
      pipeline = [
        {'$match': match},
        {'$skip': skip},
        {'$limit': limit},
      ]
      if self.user is not None:
        pipeline.append({
          '$sort': 'swaggers.%s' % str(self.user['_id'])
        })
      pipeline.append({
        '$project': self.user_public_fields,
      })
      return self.database.users.aggregate(pipeline)

  @api('/user/search_emails')
  @require_logged_in
  def users_by_emails_search(self, emails, limit = 200, offset = 0):
    """Search users for a list of emails.

    emails -- list of emails to search with.
    limit -- the maximum number of results.
    offset -- number to skip in search.
    """
    with elle.log.trace("%s: search %s emails (limit: %s)" %
                        (self.user['_id'], emails.count(), limit))
      res = self.database.users.find(
        {
          'email': {'$in': emails},
          'register_status': 'ok',
        },
        fields = ["_id"],
        limit = limit,
        skip = offset,
      )
      return {'users': res}

  @api('/user/search', method = 'POST')
  # XXX: This call is used by the waterfall which does not login. We need to
  # make an admin mode so that our servers can access calls they need.
  # @require_logged_in
  def user_search(self, text, limit = 5, offset = 0):
    """Search the ids of the users with handle or fullname matching text.

    text -- the query.
    offset -- the number of user to skip in the result (optional).
    limit -- the maximum number of match to return (optional).
    """
    # XXX: self.user in the log.
    with elle.log.trace("%s: search %s (limit: %s, offset: %s)" %
                        (self.user['_id'], text, limit, offset)):
      fields = ['_id', 'fullname', 'handle', 'public_key', 'connected_devices']
      users = [str(u['_id']) for u in self.database.users.find(
          {
            '$or' :
            [
              {'fullname' : {'$regex' : '^%s' % text,  '$options': 'i'}},
              {'handle' : {'$regex' : '^%s' % text, '$options': 'i'}},
            ],
            'register_status':'ok',
          },
          fields = fields,
          limit = limit,
          skip = offset,
        ).sort("swaggers.%s" % str(self.user['_id']), DESCENDING)]
      return self.success({'users': users})

  def extract_user_fields(self, user):
    return {
      '_id': user['_id'],
      'public_key': user.get('public_key', ''),
      'fullname': user.get('fullname', ''),
      'handle': user.get('handle', ''),
      'connected_devices': user.get('connected_devices', []),
      'status': self._is_connected(user['_id']),
    }

  @api('/user/<id_or_email>/view')
  def view(self, id_or_email):
    """Get public informations of an user by id or email.
    """
    id_or_email = id_or_email.lower()
    if '@' in id_or_email:
      user = self.user_by_email(id_or_email, ensure_existence = False)
    else:
      user = self._user_by_id(bson.ObjectId(id_or_email),
                              ensure_existence = False)
    if user is None:
      return self.fail(error.UNKNOWN_USER)
    else:
      return self.success(self.extract_user_fields(user))

  @api('/user/from_handle/<handle>/view')
  @require_logged_in
  def view_from_handle(self, handle):
    """Get user information from handle
    """
    with elle.log.trace("search user from handle: %s", handle):
      user = self.user_by_handle(handle, ensure_existence = False)
      if user is None:
        return self.fail(error.UNKNOWN_USER)
      else:
        return self.success(self.extract_user_fields(user))

  @api('/user/from_public_key')
  def view_from_publick_key(self, public_key):
    with elle.log.trace("search user from pk: %s", public_key):
      user = self.user_by_public_key(public_key)
      return self.success(self.extract_user_fields(user))

  ## ------- ##
  ## Swagger ##
  ## ------- ##
  def _increase_swag(self, lhs, rhs):
    """Increase users reciprocal swag amount.

    lhs -- the first user.
    rhs -- the second user.
    """
    with elle.log.trace("increase %s and %s mutual swag" % (lhs, rhs)):
      assert isinstance(lhs, bson.ObjectId)
      assert isinstance(rhs, bson.ObjectId)

      # lh_user = self._user_by_id(lhs)
      # rh_user = self._user_by_id(rhs)

      # if lh_user is None or rh_user is None:
      #   raise Exception("unknown user")

      for user, peer in [(lhs, rhs), (rhs, lhs)]:
        res = self.database.users.find_and_modify(
          {'_id': user},
          {'$inc': {'swaggers.%s' % peer: 1}},
          new = True)
        if res['swaggers'][str(peer)] == 1: # New swagger.
          self.notifier.notify_some(
            notifier.NEW_SWAGGER,
            message = {'user_id': res['_id']},
            recipient_ids = {peer},
          )

  @api('/user/swaggers')
  @require_logged_in
  def swaggers(self):
    user = self.user
    with elle.log.trace("%s: get his swaggers" % user['email']):
      return self.success({"swaggers" : list(user["swaggers"].keys())})

  @api('/user/add_swagger', method = 'POST')
  @require_admin
  def add_swagger(self,
                  user1: bson.ObjectId,
                  user2: bson.ObjectId):
    """Make user1 and user2 swaggers.
    This function is reserved for admins.

    user1 -- one user.
    user2 -- the other user.
    admin_token -- the admin token.
    """
    with elle.log.trace('%s: increase swag' % self):
      self._increase_swag(user1, user2,)
      return self.success()

  @api('/user/remove_swagger', method = 'POST')
  @require_logged_in
  def remove_swagger(self,
                     _id: bson.ObjectId):
    """Remove a user from swaggers.

    _id -- the id of the user to remove.
    """
    user = self.user
    with elle.log.trace("%s: remove swagger %s" % (user['_id'], _id)):
      swagez = self.database.users.find_and_modify(
        {'_id': user['_id']},
        {'$pull': {'swaggers': _id}},
        True #upsert
      )
      return self.success()

  def _notify_swaggers(self,
                       notification_id,
                       data,
                       user_id = None):
    """Send a notification to each user swaggers.

    notification_id -- the id of the notification to send.
    data -- the body of the notification.
    user_id -- emiter of the notification (optional,
               if logged in source is the user)
    """
    if user_id is None:
      user_id = self.user['_id']
    else:
      assert isinstance(user_id, bson.ObjectId)
      user = self._user_by_id(user_id)

    swaggers = set(map(bson.ObjectId, user['swaggers'].keys()))
    d = {"user_id" : user_id}
    d.update(data)
    self.notifier.notify_some(
      notification_id,
      recipient_ids = swaggers,
      message = d,
    )

  ## ---------- ##
  ## Favortites ##
  ## ---------- ##

  @api('/user/favorite', method = 'POST')
  @require_logged_in
  def favorite(self,
               user_id: bson.ObjectId):
    """Add a user to favorites

    user_id -- the id of the user to add.
    """
    query = {'_id': self.user['_id']}
    update = { '$addToSet': { 'favorites': user_id } }
    self.database.users.update(query, update)
    return self.success()

  @api('/user/unfavorite', method = 'POST')
  @require_logged_in
  def unfavorite(self,
                 user_id: bson.ObjectId):
    """remove a user to favorites

    user_id -- the id of the user to add.
    """
    query = {'_id': self.user['_id']}
    update = { '$pull': { 'favorites': user_id } }
    self.database.users.update(query, update)
    return self.success()

  ## ---- ##
  ## Edit ##
  ## ---- ##

  @api('/user/edit', method = 'POST')
  @require_logged_in
  def edit(self,
           fullname,
           handle):
    """ Edit fullname and handle.

    fullname -- the new user fullname.
    hadnle -- the new user handle.
    """
    user = self.user
    handle = handle.strip()
    # Clean the forbiden char from asked handle.
    handle = self.__generate_handle(handle, enlarge = False)
    fullname = fullname.strip()
    lw_handle = handle.lower()
    if not len(fullname) > 2:
      return self.fail(
        error.OPERATION_NOT_PERMITTED,
        "Fullname is too short",
        field = 'fullname',
        )
    if not len(lw_handle) > 2:
      return self.fail(
        error.OPERATION_NOT_PERMITTED,
        "Handle is too short",
        field = 'handle',
        )
    other = self.database.users.find_one({'lw_handle': lw_handle})
    if other is not None and other['_id'] != user['_id']:
      return self.fail(
        error.HANDLE_ALREADY_REGISTRED,
        field = 'handle',
        )
    update = {
      '$set': {'handle': handle, 'lw_handle': lw_handle, 'fullname': fullname}
    }
    self.database.users.find_and_modify(
      {'_id': user['_id']},
      update)
    return self.success()

  @api('/user/invite', method = 'POST')
  @require_logged_in
  def invite(self, email):
    """Invite a user to infinit.
    This function is reserved for admins.

    email -- the email of the user to invite.
    admin_token -- the admin token.
    """
    user = self.user
    with elle.log.trace("%s: invite %s" % (user['email'], email)):
      if regexp.EmailValidator(email) != 0:
        return self.fail(error.EMAIL_NOT_VALID)
      if self.database.users.find_one({"email": email}) is not None:
        self.fail(error.USER_ALREADY_INVITED)
      invitation.invite_user(
        email = email,
        send_mail = True,
        mailer = self.mailer,
        source = (user['fullname'], user['email']),
        database = self.database,
        sendername = user['fullname'],
        user_id = str(user['_id']),
      )
      return self.success()

  @api('/user/invited')
  @require_logged_in
  def invited(self):
    """Return the list of users invited.
    """
    return self.success({'user': list(map(lambda u: u['email'], self.database.invitations.find(
        {
          'source': self.user['email'],
        },
        fields = {'email': True, '_id': False}
    )))})

  @api('/user/self')
  @require_logged_in
  def user_self(self):
    """Return self data."""
    user = self.user
    return self.success({
      '_id': user['_id'],
      'fullname': user['fullname'],
      'handle': user['handle'],
      'email': user['email'],
      'devices': user.get('devices', []),
      'networks': user.get('networks', []),
      'identity': user['identity'],
      'public_key': user['public_key'],
      'accounts': user['accounts'],
      'remaining_invitations': user.get('remaining_invitations', 0),
      'token_generation_key': user.get('token_generation_key', ''),
      'favorites': user.get('favorites', []),
      'connected_devices': user.get('connected_devices', []),
      'status': self._is_connected(user['_id']),
      'created_at': user.get('created_at', 0),
    })

  @api('/user/minimum_self')
  @require_logged_in
  def minimum_self(self):
    """Return minimum self data.
    """
    user = self.user
    return self.success(
      {
        'email': user['email'],
        'identity': user['identity'],
      })

  @api('/user/remaining_invitations')
  @require_logged_in
  def invitations(self):
    """Return the number of invitations remainings.
    """
    return self.success(
      {
        'remaining_invitations': self.user.get('remaining_invitations', 0),
      })

  @api('/user/<id>/avatar')
  def get_avatar(self,
                 id: bson.ObjectId):
    print(id)
    user = self._user_by_id(id, ensure_existence = False)
    image = user and user.get('avatar')
    if image:
      from bottle import response
      response.content_type = 'image/png'
      return bytes(image)
    else:
      # Otherwise return the default avatar
      from bottle import static_file
      return static_file('place_holder_avatar.png', root = os.path.dirname(__file__), mimetype = 'image/png')

  @api('/user/avatar', method = 'POST')
  @require_logged_in
  def set_avatar(self):
    from bottle import request
    from io import BytesIO
    from PIL import Image
    image = Image.open(request.body)
    image.resize((256, 256), Image.ANTIALIAS)
    out = BytesIO()
    image.save(out, 'PNG')
    out.seek(0)
    import bson.binary
    self.database.users.find_and_modify(
      query = {"_id": self.user['_id']},
      update = {'$set': {'avatar': bson.binary.Binary(out.read())}})
    return self.success()

  ## ----------------- ##
  ## Connection status ##
  ## ----------------- ##
  def set_connection_status(self,
                            user_id,
                            device_id,
                            status):
    """Add or remove the device from user connected devices.

    device_id -- the id of the requested device
    user_id -- the device owner id
    status -- the new device status
    """
    with elle.log.trace("%s: %sconnected on device %s" %
                        (user_id, not status and "dis" or "", device_id)):
      assert isinstance(user_id, bson.ObjectId)
      assert isinstance(device_id, uuid.UUID)
      user = self.database.users.find_one({"_id": user_id})
      assert user is not None
      device = self.device(id = str(device_id), owner = user_id)
      assert str(device_id) in user['devices']

      connected_before = self._is_connected(user_id)
      elle.log.debug("%s: was%s connected before" %
                     (user_id, not connected_before and "n't" or ""))
      # Add / remove device from db
      update_action = status and '$addToSet' or '$pull'

      action = {update_action: {'connected_devices': str(device_id)}}

      elle.log.debug("%s: action: %s" % (user_id, action))

      self.database.users.update(
        {'_id': user_id},
        action,
        multi = False,
      )
      user = self.database.users.find_one({"_id": user_id}, fields = ['connected_devices'])

      elle.log.debug("%s: connected devices: %s" %
                     (user['_id'], user['connected_devices']))

      # Disconnect only user with an empty list of connected device.
      self.database.users.update(
          {'_id': user_id},
          {"$set": {"connected": bool(user["connected_devices"])}},
          multi = False,
      )

      # XXX:
      # This should not be in user.py, but it's the only place
      # we know the device has been disconnected.
      if status is False:
        with elle.log.trace("%s: disconnect nodes" % user_id):
          transactions = self.find_nodes(user_id = user['_id'],
                                         device_id = device_id)

          with elle.log.debug("%s: concerned transactions:" % user_id):
            for transaction in transactions:
              elle.log.debug("%s" % transaction)
              self.update_node(transaction_id = transaction['_id'],
                               user_id = user['_id'],
                               device_id = device_id,
                               node = None)
              self.notifier.notify_some(
                notifier.PEER_CONNECTION_UPDATE,
                recipient_ids = {transaction['sender_id'], transaction['recipient_id']},
                message = {
                  "transaction_id": str(transaction['_id']),
                  "devices": [transaction['sender_device_id'], transaction['recipient_device_id']],
                  "status": False
                }
              )

      self._notify_swaggers(
        notifier.USER_STATUS,
        {
          'status': self._is_connected(user_id),
          'device_id': str(device_id),
          'device_status': status,
        },
        user_id = user_id,
      )

  ## ----- ##
  ## Debug ##
  ## ----- ##

  @api('/debug', method = 'POST')
  @require_logged_in
  def message(self,
              sender_id: bson.ObjectId,
              recipient_id: bson.ObjectId,
              message):
    """Send a message to recipient as sender.

    sender_id -- the id of the sender.
    recipient_id -- the id of the recipient.
    message -- the message to be sent.
    """
    self.notifier.notify_some(
      notifier.MESSAGE,
      recipient_ids = {recipient_id},
      message = {
        'sender_id' : sender_id,
        'message': message,
      }
    )
    return self.success()
