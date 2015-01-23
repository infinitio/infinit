# -*- encoding: utf-8 -*-
"""
All constants goes here
"""
import os

DEBUG = True
ENCODING = 'utf-8'
SALT = "1nf1n17_S4l7"
SESSION_COOKIE_NAME = 'SESSIONID'
SESSION_TOKEN_NAME = 'token'
SESSION_HEADER_NAME = 'Authorization'

# Name of the mongo collection.
COLLECTION_NAME = os.environ.get("META_COLLECTION_NAME", 'meta')
MONGO_HOST = 'localhost'
MONGO_PORT = 27017

# release install path
INFINIT_AUTHORITY_PATH = os.path.join(
    os.path.abspath(os.path.dirname(__file__)),
    "infinit.auth"
)
INFINIT_APS_CERT_PATH = os.path.join(
  os.path.abspath(os.path.dirname(__file__)),
  'aps.pem',
)

INFINIT_AUTHORITY_PASSWORD = ""

MANDRILL_USERNAME = 'infinitdotio'
MANDRILL_PASSWORD = 'ca159fe5-a0f7-47eb-b9e1-2a8f03b9da86'
MANDRILL_SMTP_HOST = 'smtp.mandrillapp.com'
MANDRILL_SMTP_PORT = 587
MAILCHIMP_APIKEY = 'bffd6c617b533962e1441f0dc5e95225-us2'

META_HOST = "0"
META_PORT = 12345
TROPHONIUS_PORT = 23456
TROPHONIUS_CONTROL_PORT = 23457
TROPHONIUS_HOST = "localhost"
LONGINUS_HOST = "localhost"
LONGINUS_PORT = 9999
HEARTBEAT_PORT = 9898
APERTUS_HOST = "apertus.api.development.infinit.io"
APERTUS_PORT = 9899
STUN_PORT = 3478
