# -*- encoding: utf-8 -*-

from . import conf, mail

def _generate_code(email):
  import hashlib
  import time
  hash_ = hashlib.md5()
  hash_.update(email.encode('utf8') + str(time.time()).encode('utf8'))
  return hash_.hexdigest()


XXX_MAILCHIMP_SUCKS_TEMPLATE_SUBJECTS = {
  'invitation-beta': 'Welcome to the Infinit beta',
  'send-file': '%(sendername)s wants to share %(filename)s with you',
}

ALPHA_LIST = 'd8d5225ac7'
INVITED_LIST = '385e50ea2c'
USERBASE_LIST = 'cf5bcab5b1'

def move_from_invited_to_userbase(ghost_mail, new_mail):
  from mailsnake import MailSnake
  ms = MailSnake(conf.MAILCHIMP_APIKEY)
  #try:
  #  ms.listUnsubscribe(id = ALPHA_LIST, email_address = ghost_mail)
  #except:
  #  print("Couldn't unsubscribe", ghost_mail, "from ALPHA:")

  try:
    ms.listUnsubscribe(id = INVITED_LIST, email_address = ghost_mail)
  except:
    print("Couldn't unsubscribe", ghost_mail, "from INVITED")

  try:
    ms.listSubscribe(id = USERBASE_LIST,
             email_address = new_mail,
             double_optin = False)
  except:
    print("Couldn't subscribe", new_mail, "to USERBASE")

def invite_user(email,
                send_mail = True,
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
  if send_mail:
    mail.send_via_mailchimp(
      email,
      mail_template,
      subject,
      #  accesscode=code,
      **kw
    )
