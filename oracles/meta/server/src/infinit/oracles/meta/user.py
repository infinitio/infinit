# -*- encoding: utf-8 -*-

import bottle
import bson
import uuid

from .utils import api, require_logged_in, require_admin, hash_pasword
from . import error, notifier, regexp, invitation, conf

import os
import time
import unicodedata

#
# Users
#

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
    # handle = ''
    # for c in unicodedata.normalize('NFKD', fullname).encode('ascii', 'ignore'):
    #   if (c >= 'a'and c <= 'z') or (c >= 'A' and c <= 'Z') or c in '-_.':
    #     handle += c
    #   elif c in ' \t\r\n':
    #     handle += '.'
    handle = fullname.strip()

    if not enlarge:
      return handle

    if len(handle) < 5:
      handle += self.generate_dummy()
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
    assert isinstance(device_id, uuid.UUID)
    email = email.lower()
    user = self.database.users.find_one({
      'email': email,
      'password': hash_pasword(password),
    })
    if user is None:
      self.fail(error.EMAIL_PASSWORD_DONT_MATCH)
    query = {'id': str(device_id), 'owner': user['_id']}
    print("query", query)
    device = self.device(ensure_existence = False, **query)
    if device is None:
      device = self._create_device(id = device_id,
                                   owner = user)
      print("created device: %s" % device)
    else:
      assert str(device_id) in user['devices']

    # Remove potential leaked previous session.
    self.sessions.remove({'email': email, 'device': device['_id']})
    bottle.request.session['device'] = device['_id']
    bottle.request.session['email'] = email
    return self.success(
      {
        '_id' : self.user['_id'],
        'fullname': self.user['fullname'],
        'email': self.user['email'],
        'handle': self.user['handle'],
        'identity': self.user['identity'],
        'device_id': device['id'],
      }
    )

  @require_logged_in
  @api('/logout', method = 'POST')
  def logout(self):
    if 'email' in bottle.request.session:
      del bottle.request.session['device']
      del bottle.request.session['email']
      return self.success()
    else:
      return self.fail(error.NOT_LOGGED_IN)

  @property
  def user(self):
    email = bottle.request.session.get('email', None)
    if email is not None:
      return self.user_by_email(email)

  ## -------- ##
  ## Register ##
  ## -------- ##
  def _register(self, **kwargs):
    kwargs['connected'] = False
    user = self.database.users.save(kwargs)
    return user

  @api('/user/register', method = 'POST')
  def register(self, email, password, fullname, activation_code):
    """Register a new user.

    email -- the account email.
    password -- the client side hashed password.
    fullname -- the user fullname.
    activation_code -- the activation code.
    """
    _validators = [
      ('email', regexp.EmailValidator),
      ('fullname', regexp.HandleValidator),
      ('password', regexp.PasswordValidator),
    ]

    if self.user is not None:
      return self.fail(error.ALREADY_LOGGED_IN)

    email = email.lower()

    source = None
    if self.database.users.find_one(
      {
        'accounts': [{ 'type': 'email', 'id': email}],
        'register_status': 'ok',
      }):
      return self.fail(error.EMAIL_ALREADY_REGISTRED)
    elif activation_code.startswith('@'):
      activation_code = activation_code.upper()
      activation = self.database.activations.find_one(
        {
          'code': activation_code,
        })
      if not activation or activation['number'] <= 0:
        return self.fail(error.ACTIVATION_CODE_DOESNT_EXIST)
      ghost_email = email
      source = activation_code
    elif activation_code != 'no_activation_code':
      invit = self.database.invitations.find_one(
        {
          'code': activation_code,
          'status': 'pending',
        })
      if not invit:
        return self.fail(error.ACTIVATION_CODE_DOESNT_EXIST)
      ghost_email = invit['email']
      source = invit['source']
      invitation.move_from_invited_to_userbase(ghost_email, email)
    else:
      ghost_email = email

    ghost = self.database.users.find_one(
      {
        'accounts': [{ 'type': 'email', 'id': ghost_email}],
        'register_status': 'ghost',
      })

    if ghost:
      id = ghost['_id']
    else:
      id = self.database.users.save({})

    import papier
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
      remaining_invitations = 3, #XXX
      status = False,
      created_at = time.time(),
    )

    assert user_id == id

    if activation_code.startswith('@'):
      self.database.activations.update(
        {"_id": activation["_id"]},
        {
          '$inc': {'number': -1},
          '$push': {'registered': email},
        }
      )
    elif activation_code != 'no_activation_code':
      invit['status'] = 'activated'
      self.database.invitations.save(invit)

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
  @require_logged_in
  @api('/user/search/<text>', method = 'POST')
  def user_search(self, text, limit = 5, offset = 0):
    """Search the ids of the users with handle or fullname matching text.

    text -- the query.
    offset -- the number of user to skip in the result (optional).
    limit -- the maximum number of match to return (optional).
    """

    # While not sure it's an email or a fullname, search in both.
    users = []
    if not '@' in text:
      users = [str(u['_id']) for u in self.database.users.find(
          {
            '$or' :
            [
              {'fullname' : {'$regex' : '^%s' % text,  '$options': 'i'}},
              {'handle' : {'$regex' : '^%s' % text, '$options': 'i'}},
            ],
            'register_status':'ok',
          },
          fields = ["_id"],
          limit = limit,
          skip = offset,
          )]
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

  @api('/user/from_public_key')
  def view_from_publick_key(self, public_key):
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

  @require_logged_in
  @api('/user/swaggers')
  def swaggers(self):
    return self.success({"swaggers" : list(self.user["swaggers"].keys())})

  @require_admin
  @api('/user/add_swagger', method = 'POST')
  def add_swagger(self,
                  admin_token,
                  user1: bson.ObjectId,
                  user2: bson.ObjectId):
    """Make user1 and user2 swaggers.
    This function is reserved for admins.

    user1 -- one user.
    user2 -- the other user.
    admin_token -- the admin token.
    """
    self._increase_swag(user1, user2,)
    return self.success()

  @api('/user/remove_swagger', method = 'POST')
  @require_logged_in
  def remove_swagger(self,
                     _id: bson.ObjectId):
    """Remove a user from swaggers.

    _id -- the id of the user to remove.
    """
    swagez = self.database.users.find_and_modify(
      {'_id': self.user['_id']},
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
  @require_logged_in
  @api('/user/favorite', method = 'POST')
  def favorite(self,
               user_id: bson.ObjectId):
    """Add a user to favorites

    user_id -- the id of the user to add.
    """
    query = {'_id': self.user['_id']}
    update = { '$addToSet': { 'favorites': user_id } }
    self.database.users.update(query, update)
    return self.success()

  @require_logged_in
  @api('/user/unfavorite', method = 'POST')
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
  @require_logged_in
  @api('/user/edit', method = 'POST')
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
    if other and other['_id'] != user['_id']:
      return self.fail(
        error.HANDLE_ALREADY_REGISTRED,
        field = 'handle',
        )
    update = {
      'handle': handle,
      'lw_handle': lw_handle,
      'fullname': fullname,
    }
    self.database.users.update({'_id': user['_id']}, update)
    return self.success()

  @require_admin
  @api('/user/admin_invite', method = 'POST')
  def admin_invite(self,
                   email,
                   force = False,
                   dont_send_email = False,
                   admin_token = None):
    """Invite a user to infinit.
    This function is reserved for admins.

    email -- the email of the user to invite.
    admin_token -- the admin token.
    """
    email = email.strip()
    send_mail = not dont_send_email
    if self.database.invitations.find_one({'email': email}):
      if not force:
        return self.fail(error.UNKNOWN, "Already invited!")
      else:
        self.database.invitations.remove({'email': email})
    invitation.invite_user(
      email = email,
      send_mail = send_mail,
      mailer = self.mailer,
      source = 'infinit',
      database = self.database
    )
    return self.success()

  @require_logged_in
  @api('/user/invite', method = 'POST')
  def invite(self, email):
    """Invite a user to infinit.
    This function is reserved for admins.

    email -- the email of the user to invite.
    admin_token -- the admin token.
    """
    if regexp.EmailValidator(email) != 0:
      return self.fail(error.EMAIL_NOT_VALID)
    user = self.database.users.find_one({"email": email})
    if user is not None:
      self.fail(error.USER_ALREADY_INVITED)
    invitation.invite_user(
      email = email,
      send_mail = True,
      mailer = self.mailer,
      source = self.user['email'],
      database = self.database
    )
    return self.success()

  @require_logged_in
  @api('/user/invited')
  def invited(self):
    """Return the list of users invited.
    """
    return self.success({'user': self.database.invitations.find(
        {
          'source': self.user['email'],
        },
        fields = ['email']
    )})

  @require_logged_in
  @api('/user/self')
  def user_self(self):
    """Return self data."""
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
      'token_generation_key': self.user.get('token_generation_key', ''),
      'favorites': self.user.get('favorites', []),
      'connected_devices': self.user.get('connected_devices', []),
      'status': self._is_connected(self.user['_id']),
      'created_at': self.user.get('created_at', 0),
    })

  @require_logged_in
  @api('/user/minimum_self')
  def minimum_self(self):
    """Return minimum self data.
    """
    return self.success(
      {
        'email': self.user['email'],
        'identity': self.user['identity'],
      })

  @require_logged_in
  @api('/user/remaining_invitations')
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

  @require_logged_in
  @api('/user/avatar', method = 'POST')
  def set_avatar(self):
    from bottle import request
    from io import StringIO, BytesIO
    out = BytesIO()
    from PIL import Image
    image = Image.open(request.body)
    image.resize((256, 256))
    out = BytesIO()
    image.save(out, 'PNG')
    out.seek(0)
    import bson.binary
    self.database.users.find_and_modify(
      self.user['_id'],
      {'$set': {'avatar': bson.binary.Binary(out.read())}})
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
    assert isinstance(user_id, bson.ObjectId)
    assert isinstance(device_id, uuid.UUID)
    user = self.database.users.find_one({"_id": user_id})
    assert user is not None
    device = self.device(id = str(device_id), owner = user_id)
    assert str(device_id) in user['devices']

    connected_before = self._is_connected(user_id)
    # Add / remove device from db
    update_action = status and '$addToSet' or '$pull'

    self.database.users.update(
      {'_id': user_id},
      {update_action: {'connected_devices': str(device_id)}},
      multi = False,
    )
    user = self.database.users.find_one({"_id": user_id}, fields = ['connected_devices'])

    # Disconnect only user with an empty list of connected device.
    self.database.users.update(
        {'_id': user_id},
        {"$set": {"connected": bool(user["connected_devices"])}},
        multi = False,
    )

    # XXX:
    # This should not be in user.py, but it's the only place
    # we know the device has been disconnected.
    if status is False and connected_before is True:
      transactions = self.database.transactions.find(
        {
          "nodes.%s" % str(device_id): {"$exists": True}
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
          message = {
            "transaction_id": str(transaction['_id']),
            "devices": list(transaction['nodes'].keys()),
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
    return self.success()

  ## ----- ##
  ## Debug ##
  ## ----- ##
  @require_logged_in
  @api('/debug', method = 'POST')
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
