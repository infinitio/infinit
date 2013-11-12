# -*- encoding: utf-8 -*-

import decorator

from . import conf

from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.base import MIMEBase
from email.header import Header
from email.utils import formataddr
#from email.utils import parseaddr, formataddr
import smtplib
import json

ALPHA_LIST = 'd8d5225ac7'
INVITED_LIST = '385e50ea2c'
USERBASE_LIST = ''

class Mailer():

  def __init__(self,
               active = True):
    self.__active = active

  def is_active(method):
    def wrapper(wrapped, self, *a, **ka):
      if not self.__active:
        print("email was ignored because mailer is inactive")
        return # Return an empty func.
      return wrapped(self, *a, **ka)
    return decorator.decorator(wrapper, method)

  @is_active
  def send_via_mailchimp(self,
                         mail,
                         template_id,
                         subject,
                         from_="Infinit <no-reply@infinit.io>",
                         reply_to=None,
                         encoding='utf8',
                         **kw):
    msg = MIMEMultipart('alternative', _charset=encoding)
    msg['Subject'] = Header(subject, encoding)
    msg['From'] = Header(from_, encoding)
    # Got troubles with Header for recipient.
    msg['To'] = mail #formataddr(("", mail))
    msg['X-MC-Template'] = Header(template_id, encoding)
    msg['X-MC-MergeVars'] = Header(json.dumps(kw), encoding)
    if reply_to is not None:
      msg['Reply-To'] = "Infinit <{}>".format(reply_to)

    smtp_server = smtplib.SMTP(conf.MANDRILL_SMTP_HOST, conf.MANDRILL_SMTP_PORT)
    try:
      smtp_server.login(conf.MANDRILL_USERNAME, conf.MANDRILL_PASSWORD)
      smtp_server.sendmail(msg['From'], [msg['To']], msg.as_string())
    except:
      from traceback import print_exc
      print_exc()
    finally:
      smtp_server.quit()

  @is_active
  def send(self,
           email,
           subject,
           content,
           from_="Infinit <no-reply@infinit.io>",
           reply_to=None,
           encoding='utf8',
           attached=None):
    mail = MIMEMultipart()
    mail['Subject'] = Header(subject, encoding)
    mail['From'] = Header(from_, encoding)
    # Got troubles with Header for recipient.
    mail['To'] = email #formataddr(("", email))
    mail.attach(MIMEText(content, _charset=encoding))
    if reply_to is not None:
      mail['Reply-To'] = "Infinit <{}>".format(reply_to)

    if attached:
      file = MIMEBase('application', 'octet-stream')
      file.set_payload(attached)
      file.add_header('Content-Disposition', 'attachment; filename="logs.tar.bz"')
      file.add_header('Content-Transfer-Encoding', 'base64')
      mail.attach(file)

    smtp_server = smtplib.SMTP(conf.MANDRILL_SMTP_HOST, conf.MANDRILL_SMTP_PORT)
    try:
      smtp_server.login(conf.MANDRILL_USERNAME, conf.MANDRILL_PASSWORD)
      smtp_server.sendmail(mail['From'], [mail['To']], mail.as_string())
    finally:
      smtp_server.quit()

USER_REPORT_SUBJECT = """User Report (%(client_os)s)""".strip()
USER_REPORT_CONTENT = """

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
""".strip()

EXISTING_BACKTRACE_SUBJECT = """Crash Report (%(client_os)s)""".strip()
EXISTING_BACKTRACE_CONTENT = """

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
""".strip()


BACKTRACE_SUBJECT = """Crash report: %(signal)s in %(module)s - %(user)s""".strip()
BACKTRACE_CONTENT = """
%(user)s

---------
VERSION
---------
%(version)s

---------
BACKTRACE
---------
%(bt)s

---------
ENV
---------
%(env)s

---------
SPEC
---------
%(spec)s

---------
More
---------
%(more)s
""".strip()
