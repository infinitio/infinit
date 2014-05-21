# -*- encoding: utf-8 -*-

import bottle
import bson
import calendar
import datetime
import elle.log
import papier

from pymongo import errors, DESCENDING
from .utils import api, require_logged_in, require_admin, json_value
from . import cloud_buffer_token, error, notifier, regexp, conf, invitation, mail, transaction_status

#
# Link Generation
#
ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.LinkGeneration'

short_host = 'http://127.0.0.1:8080'
default_alphabet = '23456789abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ'
encoded_hash_length = 7
link_lifetime_days = 30 # Time that link will be valid.

class Mixin:

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

  def _get_aws_credentials(self, user, link_id):
    """
    Fetch AWS credentials.
    """
    token_maker = cloud_buffer_token.CloudBufferToken(
        user['_id'], link_id, 'ALL',
        aws_region = self.aws_region, bucket_name = self.aws_link_bucket)
    raw_creds = token_maker.generate_s3_token()

    if raw_creds is None:
      return None

    credentials = dict()
    credentials['access_key_id']     = raw_creds['AccessKeyId']
    credentials['bucket']            = self.aws_link_bucket
    credentials['expiration']        = raw_creds['Expiration']
    credentials['folder']            = link_id
    credentials['protocol']          = 'aws'
    credentials['region']            = self.aws_region
    credentials['secret_access_key'] = raw_creds['SecretAccessKey']
    credentials['session_token']     = raw_creds['SessionToken']

    return credentials

  def _make_share_link(self, hash):
    return str('%(short_host)s/link/%(hash)s' % {'short_host': short_host,
                                                 'hash': hash})

  @api('/link/<link_id>/credentials', method = 'GET')
  @require_logged_in
  def get_link_credentials(self, link_id: bson.ObjectId):
    """
    Fetch AWS credentials from DB.
    """
    with elle.log.trace('fetch AWS credentials for user (%s) and link (%s)',
                        self.user['_id'], link_id):
      user = self.user
      link = self.database.links.find_one({'_id': user['_id']})
      if link is None:
        self.not_found()
      if link['sender_id'] != user['_id']:
        self.forbidden()

      if link['aws_credentials'] is None:
        credentials = self._get_aws_credentials(user, link_id)
        if credentials is None:
          self.fail(error.UNABLE_TO_GET_AWS_CREDENTIALS)
        self.database.links.update(
          {'_id': link_id},
          {'$set': {'aws_credentials': credentials}})
        return {'aws_credentials': credentials}
      else:
        return {'aws_credentials': link['aws_credentials']}


  @api('/link/<link_id>/credentials', method = 'POST')
  @require_logged_in
  def make_link_credentials(self, link_id: bson.ObjectId):
    """
    Generate new AWS credentials and save them to the DB.
    """
    with elle.log.trace('generate AWS credentials for user (%s) and link (%s)',
                        self.user['_id'], link_id):
      user = self.user
      link = self.database.links.find_one({'_id': link_id})
      if link is None:
        self.not_found()
      if link['sender_id'] != user['_id']:
        self.forbidden()

      credentials = self._get_aws_credentials(user, link_id)
      if credentials is None:
        self.fail(error.UNABLE_TO_GET_AWS_CREDENTIALS)

      self.database.links.update(
        {'_id': link_id},
        {'$set': {'aws_credentials': credentials}})
      return {'aws_credentials': credentials}


  @api('/link', method = 'POST')
  @require_logged_in
  def link_generate(self, files, name):
    """
    Generate a link from a list of times and a message.

    name -- Name of file uploaded as it could be an archive of a group of files.
    files -- A dictionary containing the _root_ of the file structure and each
    element's size. E.g.:
      'files': [[<file name 0>, <file size 0>], ...,
                [<file name n>, <file size n>]]
    Returns the id, link and AWS credentials.
    """
    with elle.log.trace('generating a link for user (%s)' % self.user['_id']):
      user = self.user
      if len(files) == 0:
        self.bad_request('no file dictionary')
      if len(name) == 0:
        self.bad_request('no name')

      creation_time = datetime.datetime.utcnow()
      expiry_time = \
        creation_time + datetime.timedelta(days = link_lifetime_days)

      # Maintain a list of all elements in document here.
      link = {
        'aws_credentials': None,
        'click_count': 0,
        'ctime': creation_time,
        'expiry_time': expiry_time,
        'file_list': files,
        'hash': None,
        'mtime': creation_time,
        'name': name,
        'progress': 0.0,
        'sender_device_id': self.current_device['id'],
        'sender_id': user['_id'],
        'status': transaction_status.CREATED, # Use same enum as transactions.
      }

      link_id = self.database.links.insert(link)

      credentials = self._get_aws_credentials(user, link_id)
      if credentials is None:
        self.fail(error.UNABLE_TO_GET_AWS_CREDENTIALS)
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
          if attempt >= 20:
            elle.log.err('unable to generate unique link hash')
            self.abort('unable to generate link')
          attempt += 1

      res = {
        'transaction': self.__client_link(link),
        'aws_credentials': credentials,
      }
      print(res)
      return res

  def __client_link(self, link):
    mapping = {
      'id': '_id',
    }
    link = dict(
      (key, link[key in mapping and mapping[key] or key]) for key in (
        'id',
        'click_count',
        'ctime',
        'expiry_time',
        'file_list',
        'hash',
        'mtime',
        'name',
        'progress',
        'sender_device_id',
        'sender_id',
        'status',
    ))
    link['share_link'] = self._make_share_link(link['hash'])
    if link['status'] is transaction_status.FINISHED:
      link['link'] = cloud_buffer_token.generate_get_url(
        self.aws_region, self.aws_link_bucket,
        link['id'],
        link['name'],
        valid_days = link_lifetime_days)
    return link

  @api('/link/<id>', method = 'POST')
  @require_logged_in
  def link_update(self,
                  id: bson.ObjectId,
                  progress: float,
                  status: int):
    """
    Update the status of a given link.
    id -- _id of link.
    status -- Current status of link.
    """
    with elle.log.trace('updating link %s' % id):
      user = self.user
      if progress < 0.0 or progress > 1.0:
        self.bad_request('invalid progress')
      if status not in transaction_status.statuses.values():
        self.bad_request('invalid status')
      link = self.database.links.find_one({'_id': id})
      if link is None:
        self.not_found()
      if status == link['status']:
        return self.success()
      elif link['status'] in transaction_status.final:
        self.forbidden('cannot change status from %s to %s' %
                       (link['status'], status))
      if link['sender_id'] != user['_id']:
        self.forbidden()
      self.database.links.update(
        {'_id': id},
        {
          '$set':
          {
            'mtime': datetime.datetime.utcnow(),
            'progress': progress,
            'status': status,
          }
        }
      )
      return self.success()


  @api('/link/<hash>')
  def link_by_hash(self, hash):
    """
    Find and return the link related to the given hash.
    """
    with elle.log.trace('find link for hash: %s' % hash):
      link = self.database.links.find_and_modify(
        {'hash': hash},
        {'$inc': {'click_count': 1}},
        new = True,
        multi = False,
      )
      if link is None:
        self.not_found('link not found')
      if datetime.datetime.utcnow() > link['expiry_time']:
        self.not_found('link expired')
      link = self.__client_link(link)
      self.notifier.notify_some(
        notifier.LINK_TRANSACTION,
        recipient_ids = {link['sender_id']},
        message = link,
      )
      return link

  @api('/links')
  @require_logged_in
  def links_list(self,
                 offset: int = 0,
                 count: int = 100,
                 include_expired: bool = False):
    """
    Returns a list of the user's links.
    """
    user = self.user
    with elle.log.trace('links for %s offset=%s count=%s include_expired=%s' %
                        (user['_id'], offset, count, include_expired)):
      if include_expired:
        query = {'sender_id': user['_id']}
      else:
        query = {
          'sender_id': user['_id'],
          'expiry_time': {'$gt': datetime.datetime.utcnow()}
        }
      res = list()
      for link in self.database.links.aggregate([
        {'$match': query},
        {'$sort': {'creation_time': DESCENDING}},
        {'$skip': offset},
        {'$limit': count},
      ])['result']:
        res.append(self.__client_link(link))
      return {'links': res}
