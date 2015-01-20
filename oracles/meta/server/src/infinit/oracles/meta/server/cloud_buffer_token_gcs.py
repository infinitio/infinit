from oauth2client.client import SignedJwtAssertionCredentials
from httplib2 import Http
import requests
import datetime
import time
import sys
import Crypto.Hash.SHA256 as SHA256
import Crypto.PublicKey.RSA as RSA
import Crypto.Signature.PKCS1_v1_5 as PKCS1_v1_5
import base64
import urllib

import elle.log

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.CloudBufferTokenGCS'

class CloudBufferTokenGCS:
  client_email = '798530033299-s9b7qmrc99trk8uid53giuvus1o74cif@developer.gserviceaccount.com'

  with open("infinit-b9778962be3c.pem", 'rb') as f:
    private_key = f.read()

  scope = 'https://www.googleapis.com/auth/devstorage.read_write'

  default_bucket = 'io_infinit_links'
  host = 'storage.googleapis.com'

  def __init__(self, transaction_id, file_name, bucket_name = None):
    self.transaction_id = transaction_id
    self.file_name = file_name
    self.bucket = bucket_name
    if self.bucket is None:
      self.bucket = CloudBufferTokenGCS.default_bucket

  def _get_creds(self):
    credentials = SignedJwtAssertionCredentials(
      CloudBufferTokenGCS.client_email,
      CloudBufferTokenGCS.private_key,
      CloudBufferTokenGCS.scope)
    credentials.refresh(Http())
    return credentials.access_token

  # Initiate an upload and return an url usable without auth
  def get_upload_token(self):
    elle.log.log("Getting credentials")
    creds = self._get_creds()
    elle.log.log("Initiate upload")
    r = requests.post('https://%s.%s/%s/%s' % (self.bucket, CloudBufferTokenGCS.host,
                                               self.transaction_id, self.file_name),
                 headers = {
                  'Authorization' : 'Bearer ' + creds,
                  'Content-Length': '0',
                  'Content-Type': 'application/octet-stream',
                  'x-goog-resumable': 'start'
                 })
    if int(r.status_code / 100) != 2:
      elle.log.err('Error %s: %s' % (r.status_code, r.content))
      raise Exception('Error %s: %s' %(r.status_code, r.content))
    elle.log.log("Returning location")
    # FIXME DEBUG
    get_url = generate_get_url('', self.bucket, self.transaction_id, self.file_name)
    elle.log.log("GET URL: %s" %  (get_url))
    return r.headers['location']

# Return a signed url suitable to download the file
def generate_get_url(region,
                     bucket_name,
                     transaction_id,
                     file_path,
                     valid_days = 3650):
    expiration = datetime.datetime.now() + datetime.timedelta(days=valid_days)
    expiration = int(time.mktime(expiration.timetuple()))
    resource = '/%s/%s/%s' % (bucket_name, transaction_id, file_path)
    signature_string='%s\n%s\n%s\n%s\n%s' % ('GET', '', '', expiration, resource)
    shahash = SHA256.new(signature_string.encode('utf-8'))
    private_key = RSA.importKey(CloudBufferTokenGCS.private_key, passphrase='notasecret')
    signer = PKCS1_v1_5.new(private_key)
    signature_bytes = signer.sign(shahash)
    sig = base64.b64encode(signature_bytes)
    params = {
      'GoogleAccessId': CloudBufferTokenGCS.client_email,
      'Expires': str(expiration),
      'Signature': sig
    }
    params = urllib.parse.urlencode(params)
    url = 'https://%s.%s/%s/%s?%s' % (bucket_name, CloudBufferTokenGCS.host,
      transaction_id, file_path, params)
    return url