# -*- encoding: utf-8 -*-

import hashlib
import time

import meta.database

def _generate_code(mail):
    hash_ = hashlib.md5()
    hash_.update(mail.encode('utf8') + str(time.time()))
    return hash_.hexdigest()


XXX_MAILCHIMP_SUCKS_TEMPLATE_SUBJECTS = {
    'invitation-beta': 'Welcome to the Infinit beta',
    'sendfile': '%(sendername)s wants to share %(filename)s with you',
}

def move_from_invited_to_userbase(mail):
    from mailsnake import MailSnake
    ms = MailSnake(conf.MAILCHIMP_APIKEY)
    try:
        ms.listUnsubscribe(id = ALPHA_LIST, email_address = mail)
    except:
        print("Couldn't unsubscribe", mail, "from ALPHA")
    try:
        ms.listUnsubscribe(id = ALPHA_LIST, email_address = mail)
    except:
        print("Couldn't unsubscribe", mail, "from INVITED")
    try:
        ms.listSubscribe(id = USERBASE_LIST, email_address = mail, check_optin=True)
    except:
        print("Couldn't subscribe", mail, "to USERBASE")

def invite_user(mail, send_mail=True, mail_template='invitation-beta', **kw):
        code = _generate_code(mail)
        meta.database.invitations().insert({
            'email': mail,
            'status': 'pending',
            'code': code,
        })
        subject = XXX_MAILCHIMP_SUCKS_TEMPLATE_SUBJECTS[mail_template] % kw
        if send_mail:
            meta.mail.send(mail, mail_template, accesscode=code, **kw)
