# -*- encoding: utf-8 -*-

import elle.log
from . import conf, mail
import decorator

from itertools import chain

def _generate_code(email):
  import hashlib
  import time
  hash_ = hashlib.md5()
  hash_.update(email.encode('utf8') + str(time.time()).encode('utf8'))
  return hash_.hexdigest()

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Invitation'

general_lists = {
  'alpha': 'd8d5225ac7',
  'invited': '385e50ea2c',
  'userbase': 'cf5bcab5b1',
}

os_lists = {
  'windows': '1dabf1539f',
  'linux': 'df23aaa52e',
  'macosx': '5f74fee98d',
}

class Invitation:

  class List:

    def __init__(self, name, list_id):
      self.__name = name
      self.__list_id = list_id

    def subscribe(self, ms, email_address):
      try:
        ms.listSubscribe(id = self.__list_id,
                         email_address = email_address,
                         double_optin = False)
        elle.log.trace("%s: subscribed to %s (%s)" % (email_address, self.__name, self.__list_id))
        return True
      except:
        elle.log.warn("couldn't subscribe %s to %s (%s)" %  (email_address, self.__name, self.__list_id))
        return False

    def unsubscribe(self, ms, email_address):
      try:
        if isinstance(email_address, (set, list)):
          ms.listBatchUnsubscribe(id = self.__list_id, emails = email_address)
        else:
          ms.listUnsubscribe(id = self.__list_id, email_address = email_address)
        elle.log.trace("%s: unsubscribed from %s (%s)" % (email_address, self.__name, self.__list_id))
        return True
      except:
        elle.log.warn("couldn't unsubscribe %s from %s (%s)" %  (email_address, self.__name, self.__list_id))
        return False

    def contains(self, ms, email_address):
      res = ms.listMemberInfo(id = self.__list_id,
                              email_address = email_address)
      if not res['success']:
        return False
      if len(res['data']) == 0:
        return False
      return res['data'][0]['status'] == 'subscribed'

    def members(self, ms):
      res = ms.listMembers(id = self.__list_id)
      return res['data']

  def __init__(self,
               active = True):
    self.__active = active
    if self.__active:
      from mailsnake import MailSnake
      self.ms = MailSnake(conf.MAILCHIMP_APIKEY)
    self.lists = {}
    for name, list_id in chain(os_lists.items(), general_lists.items()):
      self.add_list(name = name, list_id = list_id)

  def add_list(self, name, list_id):
    with elle.log.trace("add list: %s (%s)" % (name, list_id)):
      self.lists.update({name: Invitation.List(list_id = list_id, name = name)})

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
  def subscribe(self, email, list_name = 'userbase'):
    mailing_list = self.lists.get(list_name)
    if mailing_list is not None:
      return mailing_list.subscribe(ms = self.ms,
                                    email_address = email)
    else:
      elle.log.warn("unknown list %s" % list_name)

  # XXX should be moved to another class.
  @is_active
  def unsubscribe(self, email, list_name = 'userbase'):
    mailing_list = self.lists.get(list_name)
    if mailing_list is not None:
      return mailing_list.unsubscribe(ms = self.ms,
                                      email_address = email)
    else:
      elle.log.warn("unknown list %s" % list_name)

  @is_active
  def subscribed(self, email, list_name = 'userbase'):
    mailing_list = self.lists.get(list_name)
    if mailing_list is not None:
      return mailing_list.contains(ms = self.ms,
                                   email_address = email)
    else:
      elle.log.warn("unknown list %s" % list_name)

  @is_active
  def members(self, list_name = 'userbase'):
    mailing_list = self.lists.get(list_name)
    if mailing_list is not None:
      return mailing_list.members(ms = self.ms)
    else:
      elle.log.warn("unknown list %s" % list_name)

def invite_user(email,
                mailer,
                send_email = True,
                source = ('Infinit', 'contact@infinit.io'),
                mail_template = 'send-invitation-no-file',
                database = None,
                merge_vars = None):
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
    if send_email:
      mailer.send_template(
        to = email,
        template_name = mail_template,
        reply_to = "%s <%s>" % source,
        merge_vars = merge_vars,
      )
