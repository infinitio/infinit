from httplib2 import Http
import requests
import datetime
import time
import os
import sys
import base64
import urllib
import urllib.parse
import elle.log

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.CloudBufferTokenGCS'

module_enabled = True
try:
  from oauth2client.client import SignedJwtAssertionCredentials
  import Crypto.Hash.SHA256 as SHA256
  import Crypto.PublicKey.RSA as RSA
  import Crypto.Signature.PKCS1_v1_5 as PKCS1_v1_5

except Exception as e:
  elle.log.warn("oauth2 not present, disabling google cloud storage: %s" % e)
  module_enabled = False

gcs_default_region = ''
gcs_default_buffer_bucket = 'io_infinit_buffer'
gcs_default_link_bucket = 'io_infinit_links'

gcs_pem = \
bytes("""-----BEGIN PRIVATE KEY-----
MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBALCm3D3cHlKYRygk
vRgesY39WUGeUN/sCBsVaxMuga1bCAZ6fVoh58pQEmeBpkjaVdtB0nz9ZBVoeDtR
PcfafaUW+UFXjRf2rJ3MoJ/J72mccSD08sjVX3Q9U5iydYhjZEx3uwhUcaHG6+Rq
f4xhb/49jfFmDJ/9zCopsiPBJQgfAgMBAAECgYEAqxgByrxOdirdCGmE6D6aM+8E
qwReSnL+atT0zzBFExVPEY9Dp6+dI5soKC4vUvJ9I45+AucdL4ruoG0QTGg3NbjC
XCD88TL2UdSog/xxHAQ37EvnoPwK6v04FZHdm94eXkJMQzpf9pP8EyVEaXZWb8Uw
2MDPGluTWgkUKZitkLECQQDjuLBFwtU9rdDZB3G00P3hMXuvomPPEHRvdpvwbxLG
WX1XNPG1FlBbQhyBgUIVATn9sU28df7kANqhhnEthXY3AkEAxpaoR0rtZzPIt4c4
3PQm+mclxxEUZozrRnO/t6bDc/wGvI7C69wIu4UI8j4zFtRRuC2qCDaTorXibFRb
PKEJWQJAY8eNFUQlg30hwbbNT9kzJPU1qOOSsCwZmK1z7on8xAR6MzfzoNFCLHpv
Wx90ARgkfNCvqyBYqzbklVn/RV7xSQJBAJluCPGb+DPGFIuHU+2STRMl4lAc6BAb
TCOQhk0T8OqJi4LfIcYsqCqJLFJMsBgxTjnoPfg+gm4x7JAZ1KvRF3ECQFcwSrNV
cun1SplfUKZQZywA8ueUU/ZuGj/XXwopPR5LgWW7sgkwdCklQUPjcecWEZFy/ODl
e9FGZj7sEHpPuDE=
-----END PRIVATE KEY-----
""", 'UTF-8')
class CloudBufferTokenGCS:
  client_email = '798530033299-s9b7qmrc99trk8uid53giuvus1o74cif@developer.gserviceaccount.com'

  private_key = gcs_pem

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
    if not module_enabled:
      raise Exception('GCS module is disabled')
    credentials = SignedJwtAssertionCredentials(
      CloudBufferTokenGCS.client_email,
      CloudBufferTokenGCS.private_key,
      CloudBufferTokenGCS.scope)
    credentials.refresh(Http())
    return credentials.access_token

  # Initiate an upload and return an url usable without auth
  def get_upload_token(self):
    if not module_enabled:
      raise Exception('GCS module is disabled')

    elle.log.debug("Getting credentials")
    creds = self._get_creds()
    elle.log.debug("Initiate upload")
    mimetypes = {
      "mp3": "audio/mpeg",
      "wav": "audio/wav",
      "mp4": "video/mp4",
      "m4a": "audio/mp4",
      "aac": "audio/mp4",
      "ogg": "audio/ogg",
      "oga": "audio/ogg",
      "ogv": "video/ogg",
      "webm": "video/webm",
      "avi": "video/avi",
      "mpg": "video/mpeg",
      "mpeg": "video/mpeg",
      "m4v": "video/mp4",
    }
    content_type = mimetypes.get(self.file_name.split('.')[-1], 'application/octet-stream')
    r = requests.post('https://%s.%s/%s/%s' % (self.bucket, CloudBufferTokenGCS.host,
                                               self.transaction_id, self.file_name),
                 headers = {
                  'Authorization' : 'Bearer ' + creds,
                  'Content-Length': '0',
                  'Content-Type': content_type,
                  'x-goog-resumable': 'start'
                 })
    if int(r.status_code / 100) != 2:
      elle.log.err('Error %s: %s' % (r.status_code, r.content))
      raise Exception('Error %s: %s' %(r.status_code, r.content))
    elle.log.debug("Returning location")
    # FIXME DEBUG
    return r.headers['location']

# Return a signed url suitable to download the file
def generate_get_url(region,
                     bucket_name,
                     transaction_id,
                     file_path,
                     valid_days = 3650,
                     method = 'GET'):
    expiration = datetime.datetime.now() + datetime.timedelta(days=valid_days)
    expiration = int(time.mktime(expiration.timetuple()))
    encoded_file_path = urllib.parse.quote(file_path)
    resource = '/%s/%s/%s' % (bucket_name, transaction_id, encoded_file_path)
    signature_string='%s\n%s\n%s\n%s\n%s' % (method, '', '', expiration, resource)
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
      transaction_id, encoded_file_path, params)
    return url
