#!/usr/bin/python3

import sys
import requests
import types
from urllib.parse import urlencode

from . import conf

class ShortenerException(BaseException):
  # Errors are listed here:
  # http://api.shortswitch.com/errors?apiKey=87171b48ff5487f8817021667298e059081d7cc0
  def __init__(self, uri, data):
    assert data['statusCode'] == 'ERROR'
    super().__init__(
      'shortening %s failed: %s' % (uri, data['errorMessage']))

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

class ShortSwitch:
  """
  """
  def __init__(self,
               api_key = conf.SHORT_SWITCH_ACCESS_TOKEN):
    self.host = 'api.shortswitch.com'
    self.api_key = api_key
    (major, minor, micro, releaselevel, serial) = sys.version_info
    parts = (major, minor, micro, '?')
    self.user_agent = "Python/%d.%d.%d shortswitch_api/%s" % parts

  def shorten(self, uri):
    """
    Create a shorten url for a given long url

    uri -- The long url to shorten.
    """
    params = dict(longUrl = uri)
    data = self._call('shorten', params)
    if data['statusCode'] == 'OK':
      return data['results'][uri]['shortUrl']
    raise ShortenerException(uri, data)

  def _call(self, method, params, timeout=5000):
    """
    Perform the http request.

    method -- The method (shorten).
    params -- The call parameters.
    timeout -- The http timeout (default: 5000ms).
    """
    assert self.api_key is not None

    params['format'] = params.get('format', 'json')
    params['apiKey'] = self.api_key
    params = _utf8_params(params)

    request = "http://%(host)s/%(method)s?%(params)s" % {
      'host': self.host,
      'method': method,
      'params': urlencode(params, doseq=1)
      }
    headers = {
      'User-agent': self.user_agent + ' requests.py'
    }
    res = requests.get(request, headers = headers)
    res.raise_for_status()
    return res.json()
