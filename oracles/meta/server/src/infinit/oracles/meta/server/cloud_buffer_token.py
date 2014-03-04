#!/usr/bin/env python3

import binascii
import bson
from datetime import datetime
import hashlib
import hmac
import json
import urllib.parse
import urllib.request
import xml.etree.ElementTree as etree

import elle.log

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.CloudBufferToken'

class CloudBufferToken:

  aws_host = 'sts.amazonaws.com'
  aws_uri = '/'

  aws_secret = '6VphkFJAAWXTBLrDebB+AWmPAHPkJWfIFR2KCfSa'
  aws_id = 'AKIAIPTEKRYOSJORHQMA'

  default_headers = {
    'content-type': 'application/json',
    'host': aws_host
  }

  default_parameters = {
    'Action': 'GetFederationToken',
    'DurationSeconds': str(36 * 60 * 60), # 36 hrs is AWS max.
    'Version': '2011-06-15',
    'X-Amz-Algorithm': 'AWS4-HMAC-SHA256'
  }

  def _aws_urlencode(self, data):
    return urllib.parse.urlencode(data).replace('+', '%20')

  def _list_to_dict(self, data):
    res = {}
    for element in data:
      res[element[0]] = element[1]
    return res

  def __init__(self, user_id, transaction_id, http_action,
               aws_region='us-east-1',
               bucket_name='us-east-1-buffer-infinit-io'):
    assert http_action in ['PUT', 'GET']
    elle.log.log('%s: fetching S3 %s token for transaction (%s), user_id (%s) for region: %s' %
                 (self, http_action, transaction_id, user_id, aws_region))
    self.user_id = user_id
    self.transaction_id = transaction_id
    self.http_action = http_action
    self.aws_region = aws_region
    self.aws_service = 'sts'
    self.bucket_name = bucket_name

    self.request_time = datetime.utcnow()
    self.headers = self._make_headers()
    self.key = self._make_key()
    self.parameters = self._make_parameters()
    self.request_url = self._generate_url()

    headers_dict = self._list_to_dict(self.headers)
    request = urllib.request.Request(self.request_url,
                                     headers=headers_dict, method='GET')
    try:
      res_xml = urllib.request.urlopen(request).read()
      elle.log.debug('%s: XML response: %s' % (self, res_xml))
      self.credentials = self._get_credentials(res_xml)
    except urllib.error.HTTPError as e:
      elle.log.err('%s: unable to fetch token (%s): %s' % (self, e, e.read()))

  def _get_credentials(self, xml_str):
    root = etree.fromstring(xml_str)
    aws_xml_ns = '{https://%s/doc/%s/}' % (
      CloudBufferToken.aws_host, CloudBufferToken.default_parameters['Version'])
    search_str = '%sGetFederationTokenResult/%s' % (aws_xml_ns, aws_xml_ns)
    credentials = {}
    credentials['SessionToken'] = root.find(
      '%sCredentials/%sSessionToken' % (search_str, aws_xml_ns)).text
    credentials['SecretAccessKey'] = root.find(
      '%sCredentials/%sSecretAccessKey' % (search_str, aws_xml_ns)).text
    credentials['Expiration'] = root.find(
      '%sCredentials/%sExpiration' % (search_str, aws_xml_ns)).text
    credentials['AccessKeyId'] = root.find(
      '%sCredentials/%sAccessKeyId' % (search_str, aws_xml_ns)).text
    credentials['FederatedUserId'] = root.find(
      '%sFederatedUser/%sFederatedUserId' % (search_str, aws_xml_ns)).text
    credentials['Arn'] = root.find(
      '%sFederatedUser/%sArn' % (search_str, aws_xml_ns)).text
    elle.log.debug('%s: user credentials: %s' % (self, credentials))
    return credentials


  # http://docs.aws.amazon.com/STS/latest/UsingSTS/sts-controlling-feduser-permissions.html
  def _make_policy(self):
    object_actions = []
    bucket_actions = None
    if self.http_action == 'PUT':
      object_actions.extend(['s3:PutObject'])
    elif self.http_action == 'GET':
      object_actions.extend(['s3:GetObject', 's3:DeleteObject'])
      bucket_actions = ['s3:ListBucket']

    object_statement = {
      'Effect': 'Allow',
      'Action': object_actions,
      'Resource': 'arn:aws:s3:::%s/%s/*' % (self.bucket_name,
                                            self.transaction_id)
    }
    bucket_statement = None
    if bucket_actions:
      bucket_statement = {
        'Effect': 'Allow',
        'Action': bucket_actions,
        'Resource': 'arn:aws:s3:::%s' % (self.bucket_name)
      }
    statements = [object_statement]
    if bucket_statement:
      statements.extend([bucket_statement])
    policy = {
      'Version': '2012-10-17',
      'Statement': statements
    }
    elle.log.debug('%s: %s policy: %s' % (self, self.http_action, policy))
    return policy

  def _make_headers(self):
    headers = CloudBufferToken.default_headers.copy()
    headers['x-amz-date'] = self.request_time.strftime('%Y%m%dT%H%M%SZ')
    headers = sorted(headers.items())
    elle.log.debug('%s: headers: %s' % (self, headers))
    return headers

  def _make_headers_str(self):
    headers_str = ''
    for key, value in self.headers:
      headers_str += '%s:%s\n' % (key, value)
    return headers_str[:-1]

  def _make_signed_headers_str(self):
    signed_headers = ''
    for key, value in self.headers:
      signed_headers += '%s;' % key
    return signed_headers[:-1]

  # http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html
  def _make_request(self):
    request = '%s\n%s\n%s\n%s\n\n%s\n%s' % (
      'GET',                                          # HTTP Method.
      CloudBufferToken.aws_uri,                       # Canonical URI.
      self._aws_urlencode(self.parameters),           # Canonical Query String.
      self._make_headers_str(),                       # Canonical Headers.
      self._make_signed_headers_str(),                # Signed Headers.
      hashlib.sha256(''.encode('utf-8')).hexdigest(), # Payload Hash.
    )
    elle.log.debug('%s: canonical request: %s' % (self, request))
    return request

  # http://docs.aws.amazon.com/general/latest/gr/sigv4-create-string-to-sign.html
  def _make_string_to_sign(self):
    res = '%s\n%s\n%s\n%s' % (
      'AWS4-HMAC-SHA256',                               # Algorithm.
      self.request_time.strftime('%Y%m%dT%H%M%SZ'),     # Timestamp.
      '%s/%s/%s/%s' % (                                 # Credential Scope.
        self.request_time.strftime('%Y%m%d'),   # Date.
        self.aws_region,                        # AWS Region.
        self.aws_service,                       # AWS Service.
        'aws4_request',                         # AWS Request Type.
      ),
      hashlib.sha256(self._make_request().encode('utf-8)')).hexdigest(), # Canonical Request Hash.
    )
    elle.log.debug('%s: string to sign: %s' % (self, res))
    return res

  # http://stackoverflow.com/questions/12092518/signing-amazon-getfederationtoken-in-python
  # The above code is python2 and uses the old signing method so had to be adjusted
  def _make_parameters(self):
    parameters = CloudBufferToken.default_parameters.copy()
    parameters['X-Amz-Credential'] = '%s/%s/%s/%s/%s' % (
      CloudBufferToken.aws_id,
      self.request_time.strftime('%Y%m%d'),
      self.aws_region,
      self.aws_service,
      'aws4_request',
    )
    parameters['X-Amz-Date'] = self.request_time.strftime('%Y%m%dT%H%M%SZ')
    parameters['X-Amz-SignedHeaders'] = self._make_signed_headers_str()
    parameters['Policy'] = json.dumps(self._make_policy())
    parameters['Name'] = self.user_id
    parameters = sorted(parameters.items())
    elle.log.debug('%s: parameters: %s' % (self, parameters))
    return parameters

  def _aws_sign(self, key, message):
    return hmac.new(key, message.encode('utf-8'), hashlib.sha256).digest()

  # http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
  def _make_key(self):
    key_str = 'AWS4%s' % CloudBufferToken.aws_secret
    k_date = self._aws_sign(bytes(key_str, 'utf-8'), self.request_time.strftime('%Y%m%d'))
    k_region = self._aws_sign(k_date, self.aws_region)
    k_service = self._aws_sign(k_region, self.aws_service)
    k_signing = self._aws_sign(k_service, 'aws4_request')
    return k_signing

  # http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
  def _generate_url(self):
    parameters = self.parameters
    signed_request = binascii.hexlify(
      self._aws_sign(self.key, self._make_string_to_sign()))
    completed_request = '%s&X-Amz-Signature=%s' % (
      self._aws_urlencode(parameters),
      urllib.parse.quote(signed_request),
    )
    url_string = 'https://%s:443?%s' % (CloudBufferToken.aws_host, completed_request)
    elle.log.debug('%s: url string: %s' % (self, url_string))
    return url_string
