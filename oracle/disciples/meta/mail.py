# -*- encoding: utf-8 -*-

from meta import conf

from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
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
    finally:
        smtp_server.quit()

def send(mail,
         subject,
         content,
         from_="Infinit <no-reply@infinit.io>",
         reply_to=None,
         encoding='utf8'):
    msg = MIMEText(content, _charset=encoding)
    msg['Subject'] = Header(subject, encoding)
    msg['From'] = Header(from_, encoding)
    # Got troubles with Header for recipient.
    msg['To'] = mail #formataddr(("", mail))
    if reply_to is not None:
        msg['Reply-To'] = "Infinit <{}>".format(reply_to)
    smtp_server = smtplib.SMTP(conf.MANDRILL_SMTP_HOST, conf.MANDRILL_SMTP_PORT)
    try:
        smtp_server.login(conf.MANDRILL_USERNAME, conf.MANDRILL_PASSWORD)
        smtp_server.sendmail(msg['From'], [msg['To']], msg.as_string())
    finally:
        smtp_server.quit()

BACKTRACE_SUBJECT = u"""Crash report: %(signal)s in %(module)s - %(user)s""".strip()
BACKTRACE_CONTENT = u"""
%(user)s

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
