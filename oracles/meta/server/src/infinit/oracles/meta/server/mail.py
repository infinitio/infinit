# -*- encoding: utf-8 -*-

import decorator

from . import conf
import mandrill

import elle.log
from email.header import Header
from email.mime.text import MIMEText
from email.utils import parseaddr, formataddr
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Mailer'

#from email.utils import parseaddr, formataddr
import smtplib
import json

MAILCHIMP_TEMPLATE_SUBJECTS = {
  'invitation-beta': '%(sendername)s would like to invite you to Infinit',
  'send-file': '*|sendername|* wants to share *|filename|* with you',
  'send-invitation-no-file': '%(sendername)s wants to use Infinit with you',
  'accept-file-only-offline': '%(sendername)s wants to share %(filename)s with you',
  'confirm-sign-up': 'Welcome to Infinit',
  'reconfirm-sign-up': 'Confirm your email',
  'daily-summary': 'You have *|count|* unaccepted files on Infinit',
}

ALPHA_LIST = 'd8d5225ac7'
INVITED_LIST = '385e50ea2c'
USERBASE_LIST = ''

class Mailer():

  def __init__(self,
               active = True):
    print("Mailer: ctr(%s)" % active)
    self.__active = active
    self.__mandrill = mandrill.Mandrill(apikey = conf.MANDRILL_PASSWORD)

  @property
  def active(self):
    return self.__active

  def is_active(method):
    def wrapper(wrapped, self, *a, **ka):
      if not self.active:
        elle.log.warn("email was ignored because mailer is inactive")
        return # Return an empty func.
      return wrapped(self, *a, **ka)
    return decorator.decorator(wrapper, method)

  @is_active
  def build_message(self,
                    to,
                    subject,
                    fr,
                    reply_to,
                    attachment):
    to = isinstance(to, str) and [to] or to
    assert isinstance(to, (list, tuple))
    message = {
      'to': list(map(lambda x: {'email': x}, to)),
      'subject': subject,
      'headers': {},
      'attachments': [],
    }
    from_name, from_email = parseaddr(fr)
    message['from_email'] = from_email
    message['from_name'] = from_name

    if reply_to is not None:
      message['headers']['Reply-To'] = reply_to
    if attachment is not None:
      # XXX: Only support one attachment for the moment.
      message['attachments'].append({
        'name': attachment[0],
        'content': attachment[1],
        'type': 'octet-stream' })
    return message

  @is_active
  def send(self,
           to,
           subject,
           body,
           fr = 'Infinit <no-reply@infinit.io>',
           reply_to = None,
           attachment = None):
    message = self.build_message(to = to,
                                 subject = subject,
                                 fr = fr,
                                 reply_to = reply_to,
                                 attachment = attachment)
    message['text'] = body
    self.__send(message)

  @is_active
  def send_template(self,
                    to,
                    template_name,
                    subject,
                    fr = 'Infinit <no-reply@infinit.io>',
                    reply_to = None,
                    attachment = None,
                    encoding = 'utf-8',
                    merge_vars = {}):
    message = self.build_message(to = to,
                                 subject = subject,
                                 fr = fr,
                                 reply_to = reply_to,
                                 attachment = attachment)

    # Turn human logic to mandrill logic.
    # {
    #   'em@il.com': {'name1': 'value1', 'name2': 'value2'},
    # }
    # ->
    # [{
    #   'rcpt': 'em@il.com',
    #   'vars': [{ 'name': 'name1', 'content': 'value1'},
    #            { 'name': 'name2', 'content': 'value2'}]
    # },]
    message['merge_vars'] = list(map(lambda recipient: {
      'rcpt': recipient,
      'vars': list(map(lambda data: {
        'name': data,
        'content': merge_vars[recipient][data]
        },
        merge_vars[recipient].keys()))},
      merge_vars.keys()))

    self.__send_template(template_name = template_name,
                         message = message)

  def __send(self, message):
    messenger = mandrill.Messages(self.__mandrill)
    messenger.send(messenge)

  def __send_template(self, template_name, message):
    messenger = mandrill.Messages(self.__mandrill)
    messenger.send_template(template_name = template_name,
                            template_content = [],
                            message = message)


report_templates = dict()

report_templates['user'] = {'subject': 'User Report (%(client_os)s)'.strip(),
                            'content': """
User file and .infinit directory in attached file.

OS: %(client_os)s

Infinit Version: %(version)s

User Name: %(user_name)s

-------
Message
-------
%(message)s

-----------
Environment
-----------
%(env)s

---------
More
---------
%(more)s
""".strip()}

report_templates['backtrace'] = {'subject': 'Crash Report (%(client_os)s)'.strip(),
                                 'content': """
Backtrace and state log attached.

OS: %(client_os)s

Infinit Version: %(version)s

User Name: %(user_name)s

-----------
Environment
-----------
%(env)s

----------------------
Additional Information
----------------------
%(more)s
""".strip()}

report_templates['transaction'] = {'subject': 'Transfer Failed Report (%(client_os)s)'.strip(),
                                   'content': """
.infinit directory in attached file.

OS: %(client_os)s

Infinit Version: %(version)s

User Name: %(user_name)s

-----------
Environment
-----------
%(env)s

----------------------
Additional Information
----------------------
%(more)s

""".strip()}
