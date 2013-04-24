# -*- encoding: utf-8 -*-

import hashlib
import time

import meta.database
import meta.conf

def _generate_code(mail):
    hash_ = hashlib.md5()
    hash_.update(mail.encode('utf8') + str(time.time()))
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
    ms = MailSnake(meta.conf.MAILCHIMP_APIKEY)
    #try:
    #    ms.listUnsubscribe(id = ALPHA_LIST, email_address = ghost_mail)
    #except:
    #    print("Couldn't unsubscribe", ghost_mail, "from ALPHA:")

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

def invite_user(mail, send_mail=True, mail_template='invitation-beta', **kw):
        code = _generate_code(mail)
        meta.database.invitations().insert({
            'email': mail,
            'status': 'pending',
            'code': code,
        })
        subject = XXX_MAILCHIMP_SUCKS_TEMPLATE_SUBJECTS[mail_template] % kw
        if send_mail:
            meta.mail.send_via_mailchimp(mail, mail_template, subject, accesscode=code, **kw)
