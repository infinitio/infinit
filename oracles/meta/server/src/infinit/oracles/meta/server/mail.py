# -*- encoding: utf-8 -*-

import decorator

from . import conf

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
  'invitation-with-file': '%(sendername)s wants to share %(filename)s with you',
  'send-invitation-no-file': '%(sendername)s wants to use Infinit with you',
  'accept-file-online-offline': '%(sendername)s wants to share %(filename)s with you',
  'accept-file-only-offline': '%(sendername)s wants to share %(filename)s with you',
  'confirm-sign-up': 'Welcome to Infinit',
  'reconfirm-sign-up': 'Confirm your email',
}

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
        elle.log.warn("email was ignored because mailer is inactive")
        return # Return an empty func.
      return wrapped(self, *a, **ka)
    return decorator.decorator(wrapper, method)

  @is_active
  def templated_send(self,
                     to,
                     template_id,
                     subject,
                     fr = "Infinit <no-reply@infinit.io>",
                     reply_to = None,
                     encoding = 'utf8',
                     attached = None,
                     **kw):
    with elle.log.trace('templated send: %s to %s' % (template_id, to)):
      msg = self.__build_request(to = to,
                                 fr = fr,
                                 subject = subject,
                                 encoding = encoding,
                                 reply_to = reply_to,
                                 attached = attached)
      template = Header()
      template.append(template_id, encoding)
      msg['X-MC-Template'] = template
      mergevars = Header()
      mergevars.append(json.dumps(kw), encoding)
      msg['X-MC-MergeVars'] = mergevars
      self.__send(msg)

  @is_active
  def send(self,
           to,
           subject,
           content,
           fr = "Infinit <no-reply@infinit.io>",
           encoding = 'utf-8',
           reply_to = None,
           attached = None):
    with elle.log.trace('send: %s to %s' % (fr, to)):
      msg = self.__build_request(to = to,
                                 fr = fr,
                                 subject = subject,
                                 encoding = encoding,
                                 reply_to = reply_to,
                                 attached = attached)
      content = MIMEText(content, _charset = encoding)
      msg.attach(content)
      self.__send(msg)

  def __send(self, msg):
    with elle.log.trace('contact mandril smtp server'):
      smtp_server = smtplib.SMTP(conf.MANDRILL_SMTP_HOST, conf.MANDRILL_SMTP_PORT)
      try:
        with elle.log.debug('log to smtp'):
          smtp_server.login(conf.MANDRILL_USERNAME, conf.MANDRILL_PASSWORD)
          elle.log.debug('send mail')
          smtp_server.sendmail(str(msg['From']), [str(msg['To'])], msg.as_string())
          elle.log.debug('successfully sent')
      except Exception as e:
        elle.log.warn("unable to send mail: %s" % e)
        import sys
        import traceback
        exc_type, exc_value, exc_traceback = sys.exc_info()
        print(repr(traceback.extract_tb(exc_traceback)))
      finally:
        elle.log.debug('close connection with smtp server')
        smtp_server.quit()

  def __build_request(self,
                      to,
                      fr,
                      subject,
                      attached = None,
                      encoding = 'utf-8',
                      reply_to = None):
    with elle.log.trace('build request'):
      elle.log.debug('sender: %s' % fr)
      sender_name, sender_addr = parseaddr(fr)
      fr = Header()
      fr.append(sender_name, encoding)
      fr.append(' <%s>' % sender_addr)
      elle.log.debug('recipient: %s' % to)
      recipient_name, recipient_addr = parseaddr(to)
      to = Header()
      to.append(recipient_name, encoding)
      to.append(' <%s>' % recipient_addr)
      msg = MIMEMultipart('alternative', _charset=encoding)
      msg['Subject'] = Header(subject, encoding)
      msg['From'] = fr
      msg['To'] = to
      if attached is not None:
        filename, filecontent = attached
        elle.log.debug("attachement size: %s" % len(filecontent))
        with elle.log.debug('has attachement: %s' % filename):
          elle.log.dump('attachement content: %s' % filecontent)
          attachement = MIMEBase('application', 'octet-stream')
          attachement.set_payload(filecontent)
          attachement.add_header('Content-Disposition', 'attachment; filename="%s"' % filename)
          attachement.add_header('Content-Transfer-Encoding', 'base64')
          msg.attach(attachement)
      if reply_to is not None:
        elle.log.debug('has reply to: %s' % reply_to)
        rto_name, rto_addr = parseaddr(reply_to)
        rto = Header()
        rto.append(rto_name, encoding)
        rto.append(' <%s>' % rto_addr)
        msg['Reply-To'] = rto
      return msg

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
