#!/usr/bin/python3

import sys
import requests

import types
from urllib.parse import urlencode
from . import conf

def _utf8(string):
  """
  Turn the given string to utf-8.

  string -- The input string.

  """
  if isinstance(string, str):
    string = string.encode('utf-8')
  return string

def _utf8_params(params):
  """
  Encode a dictionary of URL parameters (including iterables) as utf-8.

  params -- The dictionary of parameters.
  """
  assert isinstance(params, dict)
  encoded_params = []
  for k, v in params.items():
    if v is None:
      continue
    if isinstance(v, (int, float)):
      v = str(v)
    if isinstance(v, (list, tuple, set)):
      v = [_utf8(x) for x in v]
    else:
      v = _utf8(v)
    encoded_params.append((k, v))
  return dict(encoded_params)

class bitly:
  """bitly wrapper, adapted to our needs from bitly_api python2 package.
  """
  def __init__(self,
               access_token = conf.BITLY_ACCESS_TOKEN,
               secret = conf.BITLY_SECRET):
    self.host = 'api.bit.ly'
    self.ssl_host = 'api-ssl.bit.ly'
    (major, minor, micro, releaselevel, serial) = sys.version_info
    parts = (major, minor, micro, '?')
    self.user_agent = "Python/%d.%d.%d bitly_api/%s" % parts
    self.access_token = access_token
    self.secret = secret
    self.login = None
    self.api_key = None

  @classmethod
  def _generateSignature(self, params, secret):
    """
    Generate a signature.

    params -- The list of parameters.
    secret -- The app secret.
    """
    if not params or not secret:
      return ""
    hash_string = ""
    if not params.get('t'):
      import time
      # note, this uses a utc timestamp not a local timestamp
      params['t'] = str(int(time.mktime(time.gmtime())))

    keys = list(params.keys())
    keys.sort()
    for k in keys:
      hash_string += params[k]
    hash_string += secret
    from hashlib import md5
    signature = md5(hash_string.encode()).hexdigest()[:10]
    return signature

  def shorten(self, uri, preferred_domain = conf.BITLY_PREFERRED_DOMAIN):
    """
    Create a bitly link for a given long url

    uri -- The long url to shorten.
    preferred_domain -- The domain bit.ly[default], bitly.com, or j.mp.
    """
    params = dict(uri=uri)
    if preferred_domain:
      params['domain'] = preferred_domain
    data = self._call_oauth2(params)
    return data['data']

  def _call_oauth2(self, params):
    assert self.access_token
    return self._call(self.ssl_host, 'v3/shorten', params, self.secret)

  def _call(self, host, method, params, secret=None, timeout=5000):
    """
    Perform the http(s) to bitly's api.

    host -- The host.
    method -- The method (shorten).
    params -- The call parameters.
    secret -- (Optional) The app secret to sign the request.
    timeout -- The http timeout (default: 5000ms).
    """
    params['format'] = params.get('format', 'json')  # default to json

    if self.access_token:
      scheme = 'https'
      params['access_token'] = self.access_token
      host = self.ssl_host
    else:
      assert self.login
      assert self.api_key
      scheme = 'http'
      params['login'] = self.login
      params['apiKey'] = self.api_key
    if secret:
      params['signature'] = self._generateSignature(params, secret)

    # force to utf8 to fix ascii codec errors
    params = _utf8_params(params)

    request = "%(scheme)s://%(host)s/%(method)s?%(params)s" % {
      'scheme': scheme,
      'host': host,
      'method': method,
      'params': urlencode(params, doseq=1)
      }
    headers = {
      'User-agent': self.user_agent + ' requests.py'
    }
    res = requests.get(request, headers = headers)
    res.raise_for_status()
    data = res.json()
    return data
