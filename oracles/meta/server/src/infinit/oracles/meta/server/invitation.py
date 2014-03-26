# -*- encoding: utf-8 -*-

import elle.log
from . import conf, mail
import decorator

def _generate_code(email):
  import hashlib
  import time
  hash_ = hashlib.md5()
  hash_.update(email.encode('utf8') + str(time.time()).encode('utf8'))
  return hash_.hexdigest()

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Invitation'

ALPHA_LIST = 'd8d5225ac7'
INVITED_LIST = '385e50ea2c'
USERBASE_LIST = 'cf5bcab5b1'

class Invitation:

  def __init__(self,
               active = True):
    self.__active = active
    if self.__active:
      from mailsnake import MailSnake
      self.ms = MailSnake(conf.MAILCHIMP_APIKEY)

  def is_active(method):
    def wrapper(wrapped, self, *a, **ka):
      if not self.__active:
        elle.log.warn("invitation was ignored because inviter is inactive")
        return # Return an empty func.
      return wrapped(self, *a, **ka)
    return decorator.decorator(wrapper, method)

  @is_active
  def move_from_invited_to_userbase(self, ghost_mail, new_mail):
    try:
      self.ms.listUnsubscribe(id = INVITED_LIST, email_address = ghost_mail)
      elle.log.trace("%s: unsubscribed from INVITED: %s" % (ghost_mail, INVITED_LIST))
    except:
      elle.log.warn("Couldn't unsubscribe %s from INVITED: %s" % (ghost_mail, INVITED_LIST))
    self.subscribe(new_mail)

  @is_active
  def subscribe(self, email):
    try:
      self.ms.listSubscribe(id = USERBASE_LIST,
                            email_address = email,
                            double_optin = False)
      elle.log.trace("%s added to USERBASE_LIST: %s" % (email, USERBASE_LIST))
    except:
      elle.log.warn("Couldn't subscribe %s to USERBASE" % email)

def invite_user(email,
                mailer,
                send_email = True,
                source = ('Infinit', 'no-reply@infinit.io'),
                mail_template = 'send-invitation-no-file',
                database = None,
                **kw):
  assert isinstance(source, tuple)
  with elle.log.trace('invite user %s' % email):
    assert database is not None
    code = _generate_code(email)
    database.invitations.insert({
      'email': email,
      'status': 'pending',
      'code': code,
      'source': source[1],
    })
    subject = mail.MAILCHIMP_TEMPLATE_SUBJECTS[mail_template] % kw
    elle.log.debug('subject: %s' % subject)
    if send_email:
      mailer.templated_send(
        to = email,
        template_id = mail_template,
        subject = subject,
        reply_to = "%s <%s>" % source,
        #  accesscode=code,
        **kw
      )
