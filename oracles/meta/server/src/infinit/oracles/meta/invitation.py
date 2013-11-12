# -*- encoding: utf-8 -*-

from . import conf, mail
import decorator

def _generate_code(email):
  import hashlib
  import time
  hash_ = hashlib.md5()
  hash_.update(email.encode('utf8') + str(time.time()).encode('utf8'))
  return hash_.hexdigest()


XXX_MAILCHIMP_SUCKS_TEMPLATE_SUBJECTS = {
  'invitation-beta': '%(sendername)% would like to invite you to the Infinit',
  'send-file': '%(sendername)s wants to share %(filename)s with you',
}

ALPHA_LIST = 'd8d5225ac7'
INVITED_LIST = '385e50ea2c'
USERBASE_LIST = 'cf5bcab5b1'

class Invitation:

  def __init__(self,
               active = True):
    self.__active = active
    from mailsnake import MailSnake
    self.ms = MailSnake(conf.MAILCHIMP_APIKEY)

  def is_active(method):
    def wrapper(wrapped, self, *a, **ka):
      if not self.__active:
        print("invitation was ignored because inviter is inactive")
        return # Return an empty func.
      return wrapped(self, *a, **ka)
    return decorator.decorator(wrapper, method)

  @is_active
  def move_from_invited_to_userbase(self, ghost_mail, new_mail):
    try:
      self.ms.listUnsubscribe(id = INVITED_LIST, email_address = ghost_mail)
    except:
      print("Couldn't unsubscribe", ghost_mail, "from INVITED")
    self.subscribe(new_mail)

  @is_active
  def subscribe(self, email):
    try:
      ms.listSubscribe(id = USERBASE_LIST,
                       email_address = mail,
                       double_optin = False)
    except:
      print("Couldn't subscribe", mail, "to USERBASE")

def invite_user(email,
                mailer,
                send_email = True,
                source = 'infinit',
                mail_template = 'invitation-beta',
                database = None,
                **kw):
  assert database is not None
  code = _generate_code(email)
  database.invitations.insert({
    'email': email,
    'status': 'pending',
    'code': code,
    'source': source,
  })
  subject = XXX_MAILCHIMP_SUCKS_TEMPLATE_SUBJECTS[mail_template] % kw
  if send_email:
    mailer.send_via_mailchimp(
      email,
      mail_template,
      subject,
      #  accesscode=code,
      **kw
      )
