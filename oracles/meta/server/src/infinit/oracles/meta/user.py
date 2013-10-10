# -*- encoding: utf-8 -*-

from bson import ObjectId

from .utils import api, require_logged_in, hash_pasword
from . import error, notifier, regexp, invitation, conf

try:
  from PIL import Image
except ImportError:
  try:
    import Image
  except ImportError:
    print("Cannot import Image module, please install PIL")
    import sys
    sys.exit(1)

import os
import time
import unicodedata

class metalib:
  @staticmethod
  def generate_identity(*a, **ka):
    return None, None
#import metalib # used to generate identity.

class pythia:
  class constants:
    ADMIN_TOKEN = "admintoken"

#import pythia # used for admin token.

#
# Users
#

class Mixin:

  ## ------ ##
  ## Handle ##
  ## ------ ##
  @staticmethod
  def generate_dummy():
    import random
    t1 = ['lo', 'ca', 'ki', 'po', 'pe', 'bi', 'mer']
    t2 = ['ri', 'ze', 'te', 'sal', 'ju', 'il']
    t3 = ['yo', 'gri', 'ka', 'tro', 'man', 'et']
    t4 = ['olo', 'ard', 'fou', 'li']
    h = ''
    for t in [t1, t2, t3, t4]:
      h += t[int(random.random() * len(t))]
    return h

  def __generate_handle(self, fullname, enlarge = True):
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
      handle += generate_dummy()
    return handle

  def generate_handle(self, fullname):
    """ Generate handle from a given fullname.

    fullname -- plain text user fullname.
    """
    return self.__generate_handle(fullname)

  def unique_handle(self, fullname):
    import random
    h = self.__generate_handle(fullname)
    while self.user_by_handle(h, ensure_existence = False):
      h += str(int(random.random() * 10))
    return h

  ## ------------------------- ##
  ## Login / Logout / Register ##
  ## ------------------------- ##

  def generate_token(self, token_generation_key):
    """Generate a token for further communication

    token_generation_key -- the root of the token generator.
    """
    if self.user is not None:
      return self.fail(error.ALREADY_LOGGED_IN)

    if self.authenticate_with_token(self.data['token_generation_key']):
      return self.success({
        "_id" : self.user["_id"],
        'token': self.session.session_id,
        'fullname': self.user['fullname'],
        'email': self.user['email'],
        'handle': self.user['handle'],
        'identity': self.user['identity'],
      })
    return self.fail(error.ALREADY_LOGGED_IN)

  def __register(self, **kwargs):
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

    # Factor.
    # validators

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

    identity, public_key = metalib.generate_identity(
      str(id),
      email,
      password,
      conf.INFINIT_AUTHORITY_PATH,
      conf.INFINIT_AUTHORITY_PASSWORD
      )

    handle = self.unique_handle(fullname)

    user_id = self.__register(
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
    assert isinstance(_id, ObjectId)
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

  def is_connected(self, user_id):
    """Get the connection status of a given user.

    user_id -- the id of the user.
    """
    assert isinstance(user_id, ObjectId)
    user = self.database.users.find_one(user_id)
    self.__ensure_user_existence(user)
    connected = user.get('connected', False)
    assert isinstance(connected, bool)
    return connected

  ## ------ ##
  ## Search ##
  ## ------ ##

  @api('/user/search')
  def search(self, text, limit = 5, offset = 0):
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
    return {'users': users}

  def extract_user_fields(self, user):
    return {
      '_id': user['_id'],
      'public_key': user.get('public_key', ''),
      'fullname': user.get('fullname', ''),
      'handle': user.get('handle', ''),
      'connected_devices': user.get('connected_devices', []),
      'status': self.is_connected(user['_id']),
    }

  @api('/user/<id_or_email>/view')
  def view(self, id_or_email):
    """Get public informations of an user by id or email.
    """
    id_or_email = id_or_email.lower()
    if '@' in id_or_email:
      user = self.user_by_email(id_or_email, ensure_existence = False)
    else:
      user = self._user_by_id(ObjectId(id_or_email),
                              ensure_existence = False)
    if user is None:
      return self.fail(error.UNKNOWN_USER)
    else:
      return self.success(self.extract_user_fields(user))

  @api('/user/from_public_key')
  def view_from_publick_key(self, public_key):
    user = self.user_by_public_key(public_key)
    return self.success(self.extract_user_fields(user))

  ## ------ ##
  ## Swager ##
  ## ------ ##
  def _increase_swag(self, lhs, rhs):
    """Increase users reciprocal swag amount.

    lhs -- the first user.
    rhs -- the second user.
    """
    assert isinstance(lhs, ObjectId)
    assert isinstance(rhs, ObjectId)

    lh_user = self._user_by_id(lhs)
    rh_user = self._user_by_id(rhs)

    if lh_user is None or rh_user is None:
      raise Exception("unknown user")

    for user, peer in [(lh_user, rhs), (rh_user, lhs)]:
      user['swaggers'][str(peer)] =\
          user['swaggers'].setdefault(str(peer), 0) + 1
      self.database.users.save(user)
      if user['swaggers'][str(peer)] == 1: # New swagger.
        self.notifier.notify_some(
          notifier.NEW_SWAGGER,
          message = {'user_id': user['_id']},
          recipient_ids = [peer,],
        )

  @api('/user/swaggers')
  @require_logged_in
  def swaggers(self):
    return self.success({"swaggers" : list(self.user["swaggers"].keys())})

  @api('/user/add_swagger', method = 'POST')
  def add_swagger(self, admin_token, user1, user2):
    """Make user1 and user2 swaggers.
    This function is reserved for admins.

    user1 -- one user.
    user2 -- the other user.
    admin_token -- the admin token.
    """

    if admin_token != pythia.constants.ADMIN_TOKEN:
      return self.fail(error.UNKNOWN, "You're not admin")

    self._increase_swag(
      ObjectId(user1),
      ObjectId(user2),
    )
    return self.success({"swag": "up"})

  @api('/user/remove_swagger', method = 'POST')
  @require_logged_in
  def remove_swagger(self, _id):
    """Remove a user from swaggers.

    _id -- the id of the user to remove.
    """
    swagez = self.database.users.find_and_modify(
      {"_id": ObjectId(self.user["_id"])},
      {"$pull": {"swaggers": self.data["_id"]}},
      True #upsert
    )
    return self.success({"swaggers" : swagez["swaggers"]})

  def _notify_swaggers(self, notification_id, data, user_id = None):
    """Send a notification to each user swaggers.

    notification_id -- the id of the notification to send.
    data -- the body of the notification.
    user_id -- emiter of the notification (optional,
               if logged in source is the user)
    """
    if user_id is None:
      user_id = self.user['_id']
    else:
      assert isinstance(user_id, ObjectId)
      user = self._user_by_id(user_id)

    swaggers = map(ObjectId, user['swaggers'].keys())
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
  def favorite(self, user_id):
    """Add a user to favorites

    user_id -- the id of the user to add.
    """
    favorite = ObjectId(user_id)
    lst = self.user.setdefault('favorites', [])
    if not favorite in lst:
      lst.append(favorite)
      self.database.users.save(self.user)
    return self.success()

  @api('/user/unfavorite', method = 'POST')
  @require_logged_in
  def unfavorite(self, user_id):
    """remove a user to favorites

    user_id -- the id of the user to add.
    """
    favorite = ObjectId(user_id)
    lst = self.user.setdefault('favorites', [])
    if favorite in lst:
      lst.remove(favorite)
      self.database.users.save(self.user)
    return self.success()

  ## ---- ##
  ## Edit ##
  ## ---- ##

  @api('/user/edit', method = 'POST')
  @require_logged_in
  def edit(self, fullname, handle):
    """ Edit fullname and handle.

    fullname -- the new user fullname.
    hadnle -- the new user handle.
    """
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
    if other and other['_id'] != self.user['_id']:
      return self.fail(
        error.HANDLE_ALREADY_REGISTRED,
        field = 'handle',
        )
    # XXX/ Do that in an atomic way.
    self.user['handle'] = handle
    self.user['lw_handle'] = lw_handle
    self.user['fullname'] = fullname
    self.database.users.save(self.user)
    return self.success()

  @api('/user/invite', method = 'POST')
  def invite(self,
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
    if admin_token == pythia.constants.ADMIN_TOKEN:
      send_mail = not dont_send_email
      if self.database.invitations.find_one({'email': email}):
        if not force:
          return self.fail(error.UNKNOWN, "Already invited!")
        else:
          self.database.invitations.remove({'email': email})
      invitation.invite_user(
        email,
        send_mail = send_mail,
        source = 'infinit',
        database = self.database
      )
    else:
      if self.user is None:
        self.fail(error.NOT_LOGGED_IN)
      email = self.data['email'].strip()
      if regexp.EmailValidator(email) != 0:
        return self.fail(error.EMAIL_NOT_VALID)
      self.database.users.save(self.user)
      invitation.invite_user(
        email,
        send_mail = True,
        source = self.user['email'],
        database = self.database
      )
    return self.success()

  @api('/user/invited')
  @require_logged_in
  def invited(self):
    """Return the list of users invited.
    """
    return self.success({'user': self.database.invitations.find(
        {
          'source': self.user['email'],
        },
        fields = ['email']
    )})

  @api('/self')
  @require_logged_in
  def user_self(self):
    """Return self data."""
    return self.success({
      '_id': self.user['_id'],
      'fullname': self.user['fullname'],
      'handle': self.user['handle'],
      'email': self.user['email'],
      'devices': self.user.get('devices', []),
      'networks': self.user.get('networks', []),
      'identity': self.user['identity'] or 'PLS FIXXXXXXXX ME',
      'public_key': self.user['public_key'] or 'PLS FIXXXXXXXX ME',
      'accounts': self.user['accounts'],
      'remaining_invitations': self.user.get('remaining_invitations', 0),
      'connected_devices': self.user.get('connected_devices', []),
      'status': self.is_connected(ObjectId(self.user['_id'])),
      'token_generation_key': self.user.get('token_generation_key', ''),
      'favorites': self.user.get('favorites', []),
      'created_at': self.user.get('created_at', 0),
    })

  @api('/user/minimum_self')
  @require_logged_in
  def minimum_self(self):
    """Return minimum self data.
    """
    return self.success(
      {
        'email': self.user['email'],
        'identity': self.user['identity'] or 'PLS FIXXXXXXXX ME',
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
  def get_avatar(self, id):
    user = self._user_by_id(ObjectId(id), ensure_existence = False)
    image = user and user.get('avatar')
    if image:
      from bottle import response
      response.content_type = 'image/png'
      return bytes(image)
    else:
      # Otherwise return the default avatar
      from bottle import static_file
      return static_file('place_holder_avatar.png', root = os.path.dirname(__file__), mimetype = 'image/png')

  @api('/user/<id>/avatar', method = 'POST')
  @require_logged_in
  def set_avatar(self, id):
    from bottle import request
    from io import StringIO, BytesIO
    out = BytesIO()
    try:
      # raw_data = StringIO(str(request.body))
      # image = Image.open(raw_data)
      # out = StringIO()
      # image.resize((256, 256)).save(out, 'PNG')
      # out.seek(0)
      pass
    except:
      return self.fail(msg = "Couldn't decode the image")
    from bson.binary import Binary
    self.user['avatar'] = Binary(out.read())
    self.database.users.save(self.user)
    return self.success()

  ## ------- ##
  ## Devices ##
  ## ------- ##

  def device(self, user_id, device_id, enforce_existence = True):
    """Get the device, ensuring the owner is the right one.

    device_id -- the id of the requested device
    user_id -- the device owner id
    enforce_existence -- raise if device doesn't match id or owner.
    """
    device = self.database.devices.find_one({
        '_id': device_id,
        'owner': user_id,
        })

    if device is None and enforce_existence:
      self.raise_error(
        error.DEVICE_ID_NOT_VALID,
        "The device %s does not belong to the user %s" % (device_id, user_id)
        )

    return device

  ## ----------------- ##
  ## Connection status ##
  ## ----------------- ##
  def set_connection_status(self, user_id, device_id, status):
    """Add or remove the device from user connected devices.

    device_id -- the id of the requested device
    user_id -- the device owner id
    status -- the new device status
    """
    assert isinstance(user_id, ObjectId)
    assert isinstance(device_id, ObjectId)
    assert self.user(user_id)
    assert self.device(device_id)

    connected_before = self.is_connected(user_id)
    # Add / remove device from db
    update_action = status and '$addToSet' or '$pull'

    self.database.users.update(
        query = user_id,
        request = {
          update_action: {'connected_devices': 'device_id'},
        },
        multi = False,
    )
    user = self.user(user_id, fields = ['connected_devices'])

    # Disconnect only user with an empty list of connected device.
    self.database.users.update(
        query = user_id,
        request = {"$set": {"connected": bool(user["connected_devices"])}},
        multi = False,
    )

    # XXX:
    # This should not be in user.py, but it's the only place
    # we know the device has been disconnected.
    if status is False:
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
        message = {
          "transaction_id": str(transaction['_id']),
          "devices": list(transaction['nodes'].keys()),
          "status": False
        }
      )

    self._notify_swaggers(
      notifier.USER_STATUS,
      {
        'status': self.is_connected(user_id),
        'device_id': device_id,
        'device_status': value,
      },
      user_id = user_id,
    )

  @api('/user/connect', method = 'POST')
  def connect(self, admin_token, user_id, device_id):
      """
      Add the given device from the list of connected devices of the user.
      Should only be called by Trophonius.

      device_id -- the device id to disconnect
      user_id -- the user id to disconnect

      """
      if admin_token != pythia.constants.ADMIN_TOKEN:
        return self.fail(error.UNKNOWN, "You're not admin")
      return self.__set_connection_status(self,
                                          ObjectId(user_id),
                                          ObjectId(device_id),
                                          status = True)

  @api('/user/disconnect', method = 'POST')
  def disconnect(self, admin_token, user_id, device_id):
      """Remove the given device from the list of connected devices of the user.
      Should only be called by Trophonius.

      device_id -- the device id to disconnect
      user_id -- the user id to disconnect
      }
      """
      if admin_token != pythia.constants.ADMIN_TOKEN:
        return self.fail(error.UNKNOWN, "You're not admin")
      return self.__set_connection_status(self,
                                          ObjectId(user_id),
                                          ObjectId(device_id),
                                          status = False)

  ## ----- ##
  ## Debug ##
  ## ----- ##

  @api('/debug', method = 'POST')
  @require_logged_in
  def message(self, sender_id, recipient_id, message):
    """Send a message to recipient as sender.

    sender_id -- the id of the sender.
    recipient_id -- the id of the recipient.
    message -- the message to be sent.
    """
    self.notifier.notify_some(
      notifier.MESSAGE,
      recipient_ids = [ObjectId(self.data["recipient_id"]),],
      message = {
        'sender_id' : self.data['sender_id'],
        'message': self.data['message'],
      }
    )
    return self.success()
