# -*- encoding: utf-8 -*-

import decorator

from . import conf

import datetime # For website crash reports.
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

MANDRILL_TEMPLATE_SUBJECTS = {
  'invitation-beta': '%(sendername)s would like to invite you to Infinit',
  'send-file': '*|sendername|* wants to share *|filename|* with you',
  'send-file-url': '*|sendername|* shared *|filename|* with you',
  'send-invitation-no-file': '*|sendername|* wants to use Infinit with you',
  'accept-file-only-offline': '*|sendername|* wants to share *|filename|* with you',
  'lost-password': '[Infinit] Reset your password',
  'confirm-sign-up': 'Welcome to Infinit',
  'reconfirm-sign-up': 'Confirm your email address on Infinit',
  'daily-summary': 'You have *|count|* files waiting for you on Infinit',
  'change-email-address': 'Change the main address of your Infinit account',
}

# XXX: If the template name changes, the database will need to be updated from
# {old_template_bulk} to {new_template_bulk} on every users.
subscriptions = {
  'accept-file-only-offline': 'Incoming transaction when you are offline',
  'daily-summary': 'Summary of all your pending transaction of the day',
  'drip': 'Tips and advices about how to use the product',
}

class EmailSubscriptionNotFound(BaseException):

  def __init__(self, name):
    super().__init__()
    self.name = name

def subscription_name(name):
  """
  Return the database name of the subscription for a name.

  name -- The 'pretty' name of a subscription.
  """
  if not name in subscriptions.keys():
    raise EmailSubscriptionNotFound(name)
  return name

ALPHA_LIST = 'd8d5225ac7'
INVITED_LIST = '385e50ea2c'
USERBASE_LIST = ''

# if template_bulk is not None and
class Mailer():

  def __init__(self,
               active = True):
    self.__active = active
    if active:
      import mandrill
      self.__mandrill = mandrill.Mandrill(apikey = conf.MANDRILL_PASSWORD)
    self.__meta = None

  @property
  def meta(self):
    return self.__meta

  @meta.setter
  def meta(self, meta):
    self.__meta = meta

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
                    attachment,
                    template_bulk = None):
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
           fr = 'Infinit <contact@infinit.io>',
           reply_to = None,
           attachment = None):
    with elle.log.trace('send non-templated email to %s' % to):
      message = self.build_message(to = to,
                                   subject = subject,
                                   fr = fr,
                                   reply_to = reply_to,
                                   attachment = attachment)
      message['text'] = body
      return self.__send(message)

  @is_active
  def send_template(self,
                    to,
                    template_name,
                    fr = 'Infinit <contact@infinit.io>',
                    reply_to = None,
                    attachment = None,
                    encoding = 'utf-8',
                    merge_vars = {}):
    to = isinstance(to, str) and [to] or to
    assert isinstance(to, (list, tuple))
    with elle.log.trace('send templated email (%s) to %s' % (template_name, to)):
      # If the template can be unsubscribed.
      if template_name in subscriptions.keys():
        for recipient_email in to:
          try:
            if self.meta is not None:
              subscription = subscription_name(template_name)
              recipient = self.meta.database.users.find_one({"email": recipient_email})
              if recipient is not None:
                if subscription in recipient.get('unsubscriptions', []):
                  elle.log.debug('user %s unsubscribed for this type of email' % recipient_email)
                  to.remove(recipient_email)
                  continue
              merge_vars.setdefault(recipient_email, {}).update(
                {
                  "unsubscription_email_hash": self.meta.user_unsubscription_hash(recipient),
                  "template_bulk": template_name,
                })
            else:
              elle.log.debug('meta is none')
          except EmailSubscriptionNotFound as e:
            elle.log.warn('unknown template type %s' % template_name)

      if len(to) == 0:
        elle.log.debug('no recipient remaining')
        return

      message = self.build_message(to = to,
                                   subject = MANDRILL_TEMPLATE_SUBJECTS[template_name],
                                   fr = fr,
                                   reply_to = reply_to,
                                   attachment = attachment,
                                   template_bulk = template_name)

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

      return self.__send_template(template_name = template_name,
                                  message = message)

  def __send(self, message):
    import mandrill
    messenger = mandrill.Messages(self.__mandrill)
    res = messenger.send(message)


  def __send_template(self, template_name, message):
    import mandrill
    messenger = mandrill.Messages(self.__mandrill)
    res = messenger.send_template(template_name = template_name,
                                  template_content = [],
                                  message = message)


report_templates = dict()

report_templates['user'] = {'subject': 'User Report %(version)s (%(client_os)s)'.strip(),
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

report_templates['backtrace'] = {'subject': 'Crash Report %(version)s (%(client_os)s)'.strip(),
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

report_templates['transaction'] = {'subject': 'Transfer Failed Report %(version)s (%(client_os)s): %(message)s'.strip(),
                                   'content': """
Reason: %(message)s

Transaction ID: %(transaction_id)s

User Name: %(user_name)s

Infinit Version: %(version)s

OS: %(client_os)s

-----------
Environment
-----------
%(env)s

----------------------
Additional Information
----------------------
%(more)s

""".strip()}

report_templates['website'] = {'subject': ('Website Report (%s)' % str(datetime.datetime.utcnow())).strip(),
                               'content': """
%(message)s

""".strip()}
