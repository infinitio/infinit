# -*- encoding: utf-8 -*-

import bottle
import bson
import calendar
import datetime
import time
import elle.log
import requests

from pymongo import errors, DESCENDING
from .utils import api, require_logged_in, require_logged_in_fields, require_admin, json_value, date_time
from . import cloud_buffer_token, cloud_buffer_token_gcs, error, notifier, regexp, conf, invitation, mail, transaction_status
from .team import Team

#
# Link Generation
#
ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.LinkGeneration'

default_alphabet = '23456789abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ'
encoded_hash_length = 7
link_lifetime_days = 1 # Days that each S3 request link is valid.
link_update_window_hours = 2 # Minimum number of hours an S3 link will be valid.

class Mixin:

  def __init__(self):
    self.database.users.ensure_index(
      [('expiration_date', 1)],
      sparse = True)

  def _basex_encode(self, hex_num, alphabet = default_alphabet):
    """
    Encode a given number into a base determined by the passed alphabet.

    hex_num -- Hex number to encode.
    Returns base x encoded number.
    """
    with elle.log.trace('base%s encode hex number: %s' % (len(alphabet), hex_num)):
      num = int(hex_num, 16)
      if num == 0:
        return alphabet[0]
      res = []
      base = len(alphabet)
      while num:
        rem = num % base
        num = num // base
        res.append(alphabet[rem])
      res.reverse()
      return ''.join(res)

  def _hash_link_id(self, link_id):
    """
    Generate a unique hash for a link using a random number (seeded with time)
    and the link's id then encode it so that it's human readable.

    Truncation of SHAx to y bits provides collision resistance as would be
    expected for a length of y. When we start getting too many collisions, we
    can increase the length of the hashes or roll over them.
    """
    with elle.log.trace('hashing link_id: %s' % link_id):
      import hashlib, random
      random.seed()
      string_to_hash = str(link_id) + str(random.random())
      link_hash = hashlib.sha256(string_to_hash.encode('utf-8')).hexdigest()
      elle.log.debug('hash: %s' % link_hash)
      encoded_hash = self._basex_encode(link_hash)
      return encoded_hash[:encoded_hash_length]

  def _make_share_link(self, hash):
    return str('https://infinit.io/_/%s' % hash)

  ## ----------------- ##
  ## Cloud credentials ##
  ## ----------------- ##

  @api('/link/<link_id>/credentials', method = 'GET')
  @require_logged_in
  def get_link_credentials(self, link_id: bson.ObjectId):
    return self.__credentials(link_id, False)

  @api('/link/<link_id>/credentials', method = 'POST')
  @require_logged_in
  def make_link_credentials(self, link_id: bson.ObjectId):
    """
    Generate new cloud credentials and save them to the DB.
    """
    return self.__credentials(link_id, True)

  def __credentials(self, link_id, regenerate = False):
    with elle.log.trace(
        'fetch cloud credentials for user (%s) and link (%s)' %
        (self.user['_id'], link_id)):
      user = self.user
      link = self.database.links.find_one({'_id': link_id})
      if link is None:
        self.not_found()
      if link['status'] is transaction_status.DELETED:
        self.not_found()
      if link['sender_id'] != user['_id']:
        self.forbidden()
      if link['aws_credentials'] is None or regenerate:
        credentials = self.__generate_credentials(user, link_id)
        self.database.links.update(
          {'_id': link_id},
          {'$set': {'aws_credentials': credentials}})
        return credentials
      else:
        return link['aws_credentials']

  def __generate_credentials(self, user, link_id):
    link = self.database.links.find_one({'_id': link_id})
    file_name = link['name']
    token_maker = cloud_buffer_token_gcs.CloudBufferTokenGCS(
      link_id, file_name, self.gcs_link_bucket)
    url = token_maker.get_upload_token()
    credentials = dict()
    credentials['protocol']          = 'gcs'
    now = time.strftime('%Y-%m-%dT%H-%M-%SZ', time.gmtime())
    credentials['current_time']      = now
    credentials['expiration']        = (datetime.date.today()+datetime.timedelta(days=7)).isoformat()
    credentials['url'] = url
    return credentials

  def _link_check_quota(self, user, new_link_size):
    elle.log.trace('checking link quota for %s big link' % new_link_size)
    user_links = self.__quotas(user)['links']
    elle.log.trace('usage: %s quota: %s' % (
      user_links['used'], user_links['quota']))
    if (user_links['used'] + new_link_size) > user_links['quota']:
      self.quota_exceeded(
        {
          'error': error.LINK_STORAGE_LIMIT_REACHED[0],
          'reason': 'Link size quota of %s reached' % user_links['quota'],
          'quota': int(user_links['quota']),
          'usage': int(user_links['used']),
          'link_size': int(new_link_size)
        })

  @api('/link_empty', method = 'POST')
  @require_logged_in_fields(['quota', 'total_link_size'])
  def link_create_empty(self):
    user = self.user
    # We don't know link size yet so just check if user is currently over quota.
    self._link_check_quota(user, 0)
    link_id = self.database.links.insert({})
    return self.success({
      'created_link_id': link_id,
    })

  @api('/link/<link_id>', method = 'PUT')
  @require_logged_in_fields(['quota', 'total_link_size'])
  def link_initialize(self,
                      link_id: bson.ObjectId,
                      files,
                      name,
                      message,
                      screenshot = False,
                      password = None,
                      expiration_date: date_time = None,
                      background = None,
  ):
    return self.link_generate(
      files, name, message, screenshot,
      user = self.user,
      device = self.current_device,
      link_id = link_id,
      password = password,
      expiration_date = expiration_date,
      background = background,
    )

  @api('/link', method = 'POST')
  @require_logged_in_fields(['quota', 'total_link_size'])
  def link_generate_api(self, files, name, message, screenshot = False):
    return self.link_generate(files, name, message, screenshot,
                              user = self.user,
                              device = self.current_device)

  def link_generate(self,
                    files,
                    name,
                    message,
                    screenshot,
                    user,
                    device,
                    link_id = None,
                    password = None,
                    expiration_date = None,
                    background = None,
  ):
    """
    Generate a link from a list of files and a message.

    name -- Name of file uploaded as it could be an archive of a group of files.
    files -- A dictionary containing the _root_ of the file structure and each
    element's size. E.g.:
      'files': [[<file name 0>, <file size 0>], ...,
                [<file name n>, <file size n>]]
    message --  A string message.
    Returns the id, link and AWS credentials.
    """
    def __link_size_from_file_list(files):
      res = 0;
      for name, size in files:
        res += size
      return res

    with elle.log.trace('generating a link for user (%s)' % user['_id']):
      if len(files) == 0:
        self.bad_request('no file dictionary')
      if len(name) == 0:
        self.bad_request('no name')
      if any(p is not None
             for p in [password, expiration_date, background]):
        self.require_premium()
      link_size = __link_size_from_file_list(files)
      self._link_check_quota(user, link_size)
      team = Team.team_for_user(self, user)
      self._quota_updated_notify(user, team = team, extra_link_size = link_size)
      creation_time = self.now
      # Maintain a list of all elements in document here.
      # Do not add a None hash as this causes problems with concurrency.
      link = {
        'aws_credentials': None,
        'background': background,
        'click_count': 0,
        'ctime': creation_time,
        'expiration_date': expiration_date,
        'expiry_time': None, # Field set when a link has expired.
        'file_list':
          [{'name': file[0], 'size': int(file[1])} for file in files],
        'get_url_updated': None,
        'last_accessed': None,
        'link': None,
        'message': message,
        'mtime': creation_time,
        'name': name,
        'password': self.__link_password_hash(password),
        'paused': False,
        'progress': 0.0,
        'screenshot': screenshot,
        'sender_device_id': device['id'],
        'sender_id': user['_id'],
        'status': transaction_status.CREATED, # Use same enum as transactions.
      }
      if link_id is not None:
        self.database.links.update(
            {'_id': link_id},
            {'$set': link})
      else:
        link_id = self.database.links.save(link)
      link['_id'] = link_id
      self.__update_transaction_stats(
        user,
        counts = ['sent_link', 'sent'],
        pending = link,
        time = True)
      credentials = self.__generate_credentials(user, link_id)
      link = self.database.links.find_and_modify(
        {'_id': link_id},
        {'$set': {
          'aws_credentials': credentials,
        }},
        new = True,
      )
      # We will use the DB to ensure that our hash is unique.
      attempt = 1
      link_hash = None
      while True:
        try:
          link_hash = self._hash_link_id(link_id)
          elle.log.debug('trying to store created hash (%s), attempt: %s' %
                         (link_hash, attempt))
          link = self.database.links.find_and_modify(
            {'_id': link_id},
            {'$set': {
              'hash': link_hash,
            }},
            new = True,
          )
          break
        except errors.DuplicateKeyError:
          if attempt >= 2:
            self.report_short_link_problem(attempt)
          elif attempt >= 20:
            elle.log.err('unable to generate unique link hash')
            self.abort('unable to generate link')
          attempt += 1
      res = {
        'transaction': self.__owner_link(link),
        'aws_credentials': credentials,
      }
      self.notifier.notify_some(
        notifier.LINK_TRANSACTION,
        recipient_ids = {link['sender_id']},
        message = self.__owner_link(link),
      )
      return res

  def __owner_link(self, link):
    """
    Extract the fields needed by the link owner.
    """
    mapping = {
      'id': '_id',
      'files': 'file_list',
    }
    link['share_link'] = self._make_share_link(link['hash'])
    link['has_password'] = link.get('password') is not None
    if link.get('file_size') is None:
      total_size = 0
      for f in link['file_list']:
        total_size += f['size']
      link['size'] = total_size
    else:
      link['size'] = link.get('file_size')
    link = dict(
      (key, link.get(key in mapping and mapping[key] or key)) for key in (
        'id',
        'click_count',
        'ctime',
        'expiry_time',    # Needed until 0.9.9.
        'files',
        'has_password',
        'hash',
        'message',
        'mtime',
        'name',
        'screenshot',
        'sender_device_id',
        'sender_id',
        'share_link',
        'size',
        'status',
    ))
    if link['expiry_time'] is None: # Needed until 0.9.9.
      link['expiry_time'] = 0
    if link['screenshot'] is None: # Links prior to 0.9.37
      link['screenshot'] = False
    link['files'] = [(f['name'], f['size']) for f in link['files']]
    return link

  # Deprecated in favor of /links/<id>
  @api('/link/<id>', method = 'POST')
  @require_logged_in
  def link_update_api(self,
                      id: bson.ObjectId,
                      status: int,
                      progress: float = None,
                      pause: bool = None
  ):
    return self.success(
      self.link_update(
        id,
        status = status,
        progress = progress,
        password = False,
        expiration_date = None,
        pause = pause,
        user = self.user))

  @api('/links/<id>', method = 'POST')
  @require_logged_in
  def link_update_api(
      self,
      id: bson.ObjectId,
      message: str = None,
      progress: float = None,
      status: int = None,
      password: str = False,
      expiration_date: date_time = None,
      pause: bool = None,
  ):
    return self.link_update(
      id,
      expiration_date = expiration_date,
      message = message,
      password = password,
      pause = pause,
      progress = progress,
      status = status,
      user = self.user)

  def __link_password_hash(self, password):
    if password is None:
      return password
    import hashlib
    return hashlib.sha256(
      (password + 'infinit.io/links').encode('utf-8')).hexdigest()

  def link_update(self,
                  link,
                  expiration_date = None,
                  message = None,
                  password = False,
                  pause = None,
                  progress = None,
                  status = None,
                  user = None):
    """
    Update the status of a given link.
    """
    if not isinstance(link, dict):
      link = self.database.links.find_one({'_id': link})
    with elle.log.trace('updating link %s with status %s and progress %s' %
                        (link['_id'], status, progress)):
      if link is None or len(link) == 1:
        self.not_found({
          'reason': 'link %s does not exist' % link['_id'],
          'link': link['_id'],
        })
      if user is not None and link['sender_id'] != user['_id']:
        self.forbidden({
          'reason': 'link %s does not belong to you' % link['_id'],
          'link': link['_id'],
        })
      update = {}
      if progress is not None:
        if progress < 0.0 or progress > 1.0:
          self.bad_request('invalid progress')
        if progress != link['progress']:
          update['progress'] = progress
      if status is not None:
        if status not in transaction_status.statuses.values():
          self.bad_request({
            'reason': 'invalid status',
            'status': status,
          })
        if status is not link['status']:
          if link['status'] in transaction_status.final and \
             status is not transaction_status.DELETED:
            self.forbidden('cannot change status from %s to %s' %
                           (link['status'], status))
          update['status'] = status
          if status in transaction_status.final + [transaction_status.DELETED]:
            self.__complete_transaction_pending_stats(user, link)
            if status != transaction_status.FINISHED:
              # If there is no creds, nothing was uploaded
              if link['aws_credentials'] != None:
                # erase data
                deleter = self._generate_op_url(link, 'DELETE')
                r = requests.delete(deleter)
                if int(r.status_code/100) != 2:
                  elle.log.warn('Link deletion failed with %s on %s: %s' %
                                ( r.status_code, link['_id'], r.content))
                # The delete can fail if on aws and there was a partial
                # upload, or if on gcs if nothing was uploaded at all.
            elif not link.get('quota_counted', False): #status = FINISHED but quota not yet counted
              # Get effective size
              head = self._generate_op_url(link, 'HEAD')
              r = requests.head(head)
              if int(r.status_code/100) != 2:
                elle.log.warn('Link HEAD failed with %s on %s at %s: %s' %
                  ( r.status_code, link['_id'], head, r.content))
                update.update({
                  'quota_counted': False
                })
              else:
                file_size = int(r.headers.get('Content-Length', 0))
                update.update({
                  'file_size': file_size,
                  'quota_counted': True
                })
      if expiration_date is not None:
        update['expiration_date'] = expiration_date
      if message is not None:
        update['message'] = message
      if password is not False:
        update['password'] = self.__link_password_hash(password)
      if pause is not None:
        update['paused'] = pause
      if not update:
        return {}
      update['mtime'] = self.now
      link = self.database.links.find_and_modify(
        {'_id': link['_id']},
        {'$set': update},
        new = True,
      )
      # Only update the user's quota when the quota has been counted
      if user and status and link.get('quota_counted', False):
        self._quota_updated_notify(user, team = Team.team_for_user(self, user))
      self.notifier.notify_some(
        notifier.LINK_TRANSACTION,
        recipient_ids = {link['sender_id']},
        message = self.__owner_link(link),
      )
      return {}

  def __need_update_get_link(self, link):
    """
    Function to check if we need to update the S3 GET link.
    """
    if link['status'] is not transaction_status.FINISHED:
      return False
    elif link.get('get_url_updated', None) is None:
      return True
    else:
      time_to_update = (link['get_url_updated'] +
                        datetime.timedelta(days = link_lifetime_days,
                                           hours = -link_update_window_hours))
      if self.now >= time_to_update:
        return True
      else:
        return False

  def __client_link(self, link):
    """
    This function returns fields required for web clients.
    """
    mapping = {
      'id': '_id',
      'files': 'file_list',
      'size': 'file_size',
    }
    ret_link = dict(
      (key, link.get(key in mapping and mapping[key] or key))
      for key in [
          'id',
          'background',
          'click_count',
          'ctime',
          'expiration_date',
          'files',
          'message',
          'mtime',
          'name',
          'progress',
          'sender_id',
          'sender_device_id',
          'size',
          'status',
      ])
    # Should ALWAYS exists.
    owner = self.user_by_id(link['sender_id'], fields = ['plan', 'accounts'],
                            ensure_existence = False)
    ret_link['owner_premium'] = self.premium_or_team(owner) if owner else False
    if link.get('link') is not None:
      ret_link['link'] = link['link']
    ret_link['password'] = link.get('password') is not None # deprecated in favour of 'has_password'
    ret_link['has_password'] = link.get('password') is not None
    if ret_link['size'] is None:
      total_size = 0
      for f in link['file_list']:
        total_size += f['size']
      ret_link['size'] = total_size
    return ret_link

  @api('/links/<id_or_hash>')
  def link_by_hash_api(self, id_or_hash,
                       password = None,
                       no_count: bool = False,
                       custom_domain = None):
    try:
      link_id = bson.ObjectId(id_or_hash)
      link = self.database.links.find_one({'_id': link_id})
      if link is None:
        self.not_found({
          'reason': 'link not found',
          'hash': hash,
        })
      if self.admin:
        return self.__owner_link(link)
      return self.__client_link(link)
    except bson.errors.InvalidId:
      return self.link_by_hash(
        hash = id_or_hash,
        password = password,
        no_count = no_count,
        custom_domain = custom_domain,
      )

  # Deprecated in favor of /links/<hash>
  @api('/link/<hash>')
  def link_by_hash(self, hash,
                   password = None,
                   no_count: bool = False,
                   custom_domain = None):
    """
    Find and return the link related to the given hash for a web client.
    """
    with elle.log.trace('find link for hash %s' % hash):
      link = self.database.links.find_one({'hash': hash})
      if link is None:
        self.not_found({
          'reason': 'link not found',
          'hash': hash,
        })
      owner = self.database.users.find_one(
        {'_id': link['sender_id']},
        fields = ['plan',
                  'account.custom_domains',
                  'account.default_background'])
      team = Team.team_for_user(self, owner)
      if custom_domain is not None:
        if team:
          settings = team.get('shared_settings')
        else:
          self.require_premium(owner)
          settings = owner.get('account')
        domains = (d['name']
                   for d in (settings.get('custom_domains', ())
                             if settings is not None else ()))
        if custom_domain not in domains:
          self.payment_required({
            'reason': 'invalid custom domain: %s' % custom_domain,
            'custom-domain': custom_domain,
          })
      if link['status'] is transaction_status.DELETED:
        self.gone({
          'reason': 'link was deleted',
          'hash': hash,
        })
      elif (link['expiry_time'] is not None and
            self.now > link['expiry_time']):
              self.not_found(
                {
                  'reason': 'link expired',
                  'hash': hash,
                })
      user = self.user
      mine = user is not None and user['_id'] == link['sender_id']
      if not mine and link.get('password'):
        if password is None or \
           self.__link_password_hash(password) != link['password']:
          self.unauthorized({
            'reason': 'wrong password',
            'hash': hash,
          })
      time_now = self.now
      set_dict = dict()
      set_dict['last_accessed'] = time_now
      if self.__need_update_get_link(link):
        proto = link['aws_credentials']['protocol']
        if proto == 'aws':
          link['link'] = cloud_buffer_token.generate_get_url(
            self.aws_region, self.aws_link_bucket,
            link['_id'],
            link['name'],
            valid_days = link_lifetime_days)
        else:
          link['link'] = cloud_buffer_token_gcs.generate_get_url(
            self.gcs_region, self.gcs_link_bucket,
            link['_id'],
            link['name'],
            valid_days = link_lifetime_days)
        set_dict['link'] = link['link']
        set_dict['get_url_updated'] = time_now
      if no_count:
        link = self.database.links.find_and_modify(
          {'_id': link['_id']},
          {
            '$set': set_dict,
          },
          new = True,
          multi = False,
        )
      else:
        link = self.database.links.find_and_modify(
          {'_id': link['_id']},
          {
            '$inc': {'click_count': 1},
            '$set': set_dict,
          },
          new = True,
          multi = False,
        )
        if link['click_count'] == 1:
          self.__update_transaction_stats(
            link['sender_id'],
            counts = ['reached_link', 'reached'],
            time = False)
      web_link = self.__client_link(link)
      owner_link = self.__owner_link(link)
      self.notifier.notify_some(
        notifier.LINK_TRANSACTION,
        recipient_ids = {link['sender_id']},
        message = owner_link,
      )
      if web_link.get('background') is None:
        if team:
          background = team.get('shared_settings', {}).get('default_background')
        else:
          background = owner.get('account', {}).get('default_background')
        web_link['background'] = background
      return web_link

  @api('/links')
  @require_logged_in_fields(['quota', 'total_link_size'])
  def links_list(self,
                 mtime = None,
                 offset: int = 0,
                 count: int = 500,
                 include_failed: bool = False,
                 include_deleted: bool = False,
                 include_canceled: bool = False,
                 include_expired: bool = False,
  ):
    """
    Returns a list of the user's links.
    offset -- offset of results.
    count -- number of results.
    include_expired -- should include expired links.
    """
    user = self.user
    with elle.log.trace('links for %s offset=%s count=%s include_expired=%s' %
                        (user['_id'], offset, count, include_expired)):
      query = {
        'sender_id': user['_id'],
        # Links with no hash have been interupted while
        # created. FIXME: garbage collect them.
        'hash': {'$exists': True},
        'aws_credentials': {'$exists': True},
      }
      if mtime:
         query.update({'mtime': {'$gt': mtime}})
      if not include_expired:
        query['$or'] = [
          {'expiry_time': None},
          {'expiry_time': {'$gt': self.now}},
        ]
      nin = []
      if not include_deleted:
        nin.append(transaction_status.FAILED)
      if not include_deleted:
        nin.append(transaction_status.DELETED)
      if not include_canceled:
        nin.append(transaction_status.CANCELED)
      if nin:
        query['status'] = {'$nin': nin}
      res = list()
      for link in self.database.links.aggregate([
        {'$match': query},
        {'$sort': {'ctime': DESCENDING}},
        {'$skip': offset},
        {'$limit': count},
      ])['result']:
        res.append(self.__owner_link(link))
      return {'links': res,
        'total_link_size': user.get('total_link_size', 0),
        'quota': user.get('quota', {}).get('total_link_size', 'unlimited'),
      }

  # Used when a user deletes their account.
  def delete_all_links(self, user):
    links = self.database.links.find(
      {
        'sender_id': user['_id'],
        'status': {'$nin': [transaction_status.DELETED]}
      },
      fields = ['aws_credentials', 'name'])
    for link in links:
      deleter = self._generate_op_url(link, 'DELETE')
      r = requests.delete(deleter)
      if int(r.status_code/100) != 2:
        elle.log.warn('Link deletion failed with %s on %s: %s' %
          ( r.status_code, link['_id'], r.content))
    self.database.links.update(
      {
        'sender_id': user['_id'],
        'status': {'$nin': [transaction_status.DELETED]}
      },
      {
        '$set': {
          'status': transaction_status.DELETED,
          'mtime': self.now,
        }
      },
      multi = True)

  def __link_usage(self, user):
    team = Team.team_for_user(self, user)
    if team:
      return team.storage_used
    else:
      res = self.database.links.aggregate([
        {
          '$match': {
            'sender_id': user['_id'],
            'status': transaction_status.FINISHED,
            'quota_counted': True,
          }
        },
        {
          '$group': {'_id': None, 'total':{'$sum': '$file_size'}}
        }])['result']
      total = res[0]['total'] if len(res) else 0
      return int(total)

  # Generate url on storage for given HTTP operation
  def _generate_op_url(self, link, op):
    proto = link['aws_credentials']['protocol']
    if proto == 'aws':
      res = cloud_buffer_token.generate_get_url(
        self.aws_region, self.aws_link_bucket,
        link['_id'],
        link['name'],
        valid_days = link_lifetime_days,
        method = op)
    else:
      res = cloud_buffer_token_gcs.generate_get_url(
        self.gcs_region, self.gcs_link_bucket,
        link['_id'],
        link['name'],
        valid_days = link_lifetime_days,
        method = op)
    return res

  @api('/adm/link/<id>/delete')
  @require_admin
  def adm_link_delete(self, id: bson.ObjectId):
    user_id = self.database.links.find_one({'_id': id})['sender_id']
    user = self.database.users.find_one(user_id)
    self.link_update(id, user, progress = 1, status =
                     transaction_status.DELETED)

  @api('/stats/links')
  @require_admin
  def adm_link_stats(self):
    count = self.database.links.count()
    r = self.database.links.aggregate([
      {'$match': {'quota_counted': True}},
      {'$group': { '_id': 1, 'total':{'$sum': '$file_size'}, 'max': {'$max': '$file_size'}}}
    ])['result'][0]
    total_size = r['total']
    max_size = r['max']
    over_quota = self.database.users.aggregate([
        {'$match': {
          'quota.total_link_size': {'$exists': True},
          'total_link_size': {'$exists': True},
        }},
        {'$project': {
          'oq': {'$cond': [
              {'$gt': ['$total_link_size', '$quota.total_link_size']},
          1, 0]}
        }},
        {'$match': {'oq': 1}}
    ])['result']
    over_quota = len(over_quota)
    return {
      'link_count' : count,
      'link_total_size' : total_size,
      'link_max_size': max_size,
      'user_over_quota': over_quota,
    }

  def link_cleanup_expired(self):
    expired = self.database.links.find({
      'expiration_date': {'$lt': self.now},
    })
    for link in expired:
      self.link_update(link = link,
                       status = transaction_status.DELETED)
