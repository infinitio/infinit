import requests
import json
from io import StringIO
from hashlib import sha256
import hmac
from . import error
import urllib

app_secret = '5779a2aa495c253bd867d2fb3b528c42'
app_id = '839001662829159'

class FacebookGraph:

  class AuthenticationFailure(error.Error):

    def __init__(self):
      super().__init__(error.EMAIL_PASSWORD_DONT_MATCH)

  def __init__(self, domain, initialize = False):
    self.domain = domain
    self.__app_access_token = None
    if initialize:
      # Force evaluation.
      self.app_access_token

  @property
  def app_access_token(self):
    if self.__app_access_token is None:
      # Ability to change domain is for mostly for debugging purposes.
      url = '%(domain)s/oauth/access_token' \
            '?client_id=%(app_id)s' \
            '&client_secret=%(app_secret)s' \
            '&grant_type=client_credentials' % {
              'domain': self.domain,
              'app_id': app_id,
              'app_secret': app_secret,
            }
      try:
        response = requests.get(url)
        response.raise_for_status()
        text = response.text
        if text.startswith('access_token='):
          self.__app_access_token = text[13:]
        else:
          raise FacebookGraph.AuthenticationFailure()
      except requests.exceptions.HTTPError as e:
        raise FacebookGraph.AuthenticationFailure()
    return self.__app_access_token

  class Client:

    class UserAuthenticationFailure(error.Error):

      def __init__(self):
        super().__init__(error.EMAIL_PASSWORD_DONT_MATCH)

    def __init__(self, server, code):
      self.__server = server
      self.__code = code
      self.__access_token = None
      self.__data = None
      self.__me = None
      self.__friends = None
      self.__appsecret_proof = None

    @property
    def access_token(self):
      if self.__access_token is None:
        url = '%(domain)s/oauth/access_token' \
              '?client_id=%(app_id)s' \
              '&redirect_uri=https://www.facebook.com/connect/login_success.html' \
              '&client_secret=%(app_secret)s' \
              '&code=%(code)s' % {
          'domain': self.__server.domain,
          'code': self.__code,
          'app_id': app_id,
          'app_secret': app_secret
        }
        try:
          response = requests.get(url)
          response.raise_for_status()
          text = response.text
          if text.startswith('access_token='):
            self.__access_token = text[13:text.find("&")]
          else:
            raise FacebookGraph.Client.UserAuthenticationFailure()
        except requests.exceptions.HTTPError as e:
          raise FacebookGraph.Client.UserAuthenticationFailure()
      return self.__access_token

    @access_token.setter
    def access_token(self, token):
      self.__access_token = token

    @property
    def appsecret_proof(self):
      if self.__appsecret_proof is None:
        f = hmac.new(app_secret.encode(),
                     msg = self.access_token.encode(),
                     digestmod = sha256)
        self.__appsecret_proof = str(f.hexdigest())
      return self.__appsecret_proof

    @property
    def me(self):
      if self.__me is None:
        url = '%(domain)s/v2.2/me' \
               '?access_token=%(access_token)s' \
               '&appsecret_proof=%(appsecret_proof)s' % {
                 'domain': self.__server.domain,
                 'appsecret_proof': self.appsecret_proof,
                 'access_token': self.access_token,
               }
        try:
          response = requests.get(url)
          response.raise_for_status()
          if 'id' in response.json().keys():
            self.__me = response.json()
          else:
            raise FacebookGraph.Client.UserAuthenticationFailure()
        except:
          raise FacebookGraph.Client.UserAuthenticationFailure()
      return self.__me

    @property
    def data(self):
      return self.me

    @property
    def friends(self):
      if self.__friends is None:
        url = '%(domain)s/v2.2/me/friends' \
              '?access_token=%(access_token)s' \
              '&appsecret_proof=%(appsecret_proof)s' % {
                'domain': self.__server.domain,
                'appsecret_proof': self.appsecret_proof,
                'access_token': self.access_token,
              }
        try:
          response = requests.get(url)
          response.raise_for_status()
          if 'data' in response.json().keys():
            self.__friends = response.json()
          else:
            raise FacebookGraph.Client.UserAuthenticationFailure()
        except requests.exceptions.HTTPError as e:
          raise FacebookGraph.Client.UserAuthenticationFailure()
      return self.__friends

    @property
    def facebook_id(self):
      return self.data['id']

    @property
    def avatar(self):
      url = '%(domain)s/v2.2/me/picture' \
            '?access_token=%(access_token)s' \
            '&appsecret_proof=%(appsecret_proof)s' \
            '&height=256'\
            '&width=256' % {
              'domain': self.__server.domain,
              'appsecret_proof': self.appsecret_proof,
              'access_token': self.access_token,
            }
      try:
        response = requests.get(url)
        response.raise_for_status()
        return response.content
      except requests.exceptions.HTTPError as e:
        raise FacebookGraph.Client.UserAuthenticationFailure()

    @property
    def friend_list(self):
      pass

    @property
    def permissions(self):
      pass

  def user(self, code):
    return FacebookGraph.Client(server = self, code = code)
