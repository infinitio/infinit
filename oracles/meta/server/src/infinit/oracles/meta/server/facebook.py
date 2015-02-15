import requests
import json
from io import StringIO
from hashlib import sha256
import hmac
from . import error

app_secret = '84e706b54659497ad23fb27eb9f0831d'
app_id = '1599363463620738'

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
      except HTTPError as e:
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
        except HTTPError as e:
          raise FacebookGraph.Client.UserAuthenticationFailure()
      return self.__access_token

    @access_token.setter
    def access_token(self, token):
      self.__access_token = token

    @property
    def data(self):
      if self.__data is None:
        f = hmac.new('84e706b54659497ad23fb27eb9f0831d'.encode(),
                     msg = self.access_token.encode(),
                     digestmod = sha256)
        url = '%(domain)s/v2.2/%(user_id)s' \
               '?access_token=%(access_token)s' \
               '&appsecret_proof=%(appsecret_proof)s' % {
                 'domain': self.__server.domain,
                 'user_id': self.me['id'],
                 'appsecret_proof': str(f.hexdigest()),
                 'access_token': self.access_token,
               }
        try:
          response = requests.get(url)
          response.raise_for_status()
          if 'id' in response.json().keys():
            self.__data = response.json()
          else:
            raise FacebookGraph.Client.UserAuthenticationFailure()
        except HTTPError as e:
          raise FacebookGraph.Client.UserAuthenticationFailure()
      return self.__data

    @property
    def me(self):
      if self.__me is None:
        f = hmac.new('84e706b54659497ad23fb27eb9f0831d'.encode(),
                     msg = self.access_token.encode(),
                     digestmod = sha256)
        url = '%(domain)s/v2.2/me' \
               '?access_token=%(access_token)s' \
               '&appsecret_proof=%(appsecret_proof)s' % {
                 'domain': self.__server.domain,
                 'appsecret_proof': str(f.hexdigest()),
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
    def friends(self):
      if self.__friends is None:
        f = hmac.new(app_secret.encode(),
                     msg = self.access_token.encode(),
                     digestmod = sha256)
        url = '%(domain)s/v2.2/me/friends' \
              '?access_token=%(access_token)s' \
              '&appsecret_proof=%(appsecret_proof)s' % {
                'domain': self.__server.domain,
                'appsecret_proof': str(f.hexdigest()),
                'access_token': self.access_token,
              }
        try:
          response = requests.get(url)
          response.raise_for_status()
          if 'data' in response.json().keys():
            self.__friends = response.json()
          else:
            raise FacebookGraph.Client.UserAuthenticationFailure()
        except HTTPError as e:
          raise FacebookGraph.Client.UserAuthenticationFailure()
      return self.__friends

    @property
    def id(self):
      return self.data['id']

    @property
    def friend_list(self):
      pass

    @property
    def permissions(self):
      pass

  def user(self, code):
    return FacebookGraph.Client(server = self, code = code)
