# -*- encoding: utf-8 -*-

from meta import conf

from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.MIMEBase import MIMEBase
from email.header import Header
from email.Utils import formataddr
#from email.utils import parseaddr, formataddr
import smtplib
import json

ALPHA_LIST = 'd8d5225ac7'
INVITED_LIST = '385e50ea2c'
USERBASE_LIST = ''

def send_via_mailchimp(mail,
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

def send(email,
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

USER_REPORT_SUBJECT = u"""User Report (%(client_os)s)""".strip()
USER_REPORT_CONTENT = u"""

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

""".strip()

EXISTING_BACKTRACE_SUBJECT = u"""Crash Report (%(client_os)s)""".strip()
EXISTING_BACKTRACE_CONTENT = u"""

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


BACKTRACE_SUBJECT = u"""Crash report: %(signal)s in %(module)s - %(user)s""".strip()
BACKTRACE_CONTENT = u"""
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
