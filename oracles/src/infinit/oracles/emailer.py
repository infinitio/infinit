from elle.log import log, warn, err, trace, debug, dump
from infinit.oracles.utils import key

import json

ELLE_LOG_COMPONENT = 'infinit.oracles.emailer'


class Emailer:

  pass


class NoopEmailer(Emailer):

  def send_one(self,
               template,
               recipient_email,
               recipient_name = None,
               sender_email = None,
               sender_name = None,
               reply_to = None,
               variables = None,
             ):
    pass


class SendWithUsEmailer(Emailer):

  def __init__(self, api_key):
    import sendwithus
    import sendwithus.encoder
    class JSONEncoder(sendwithus.encoder.SendwithusJSONEncoder):
      def default(self, obj):
        import bson
        import datetime
        if isinstance(obj, datetime.datetime):
          return {
            'year': obj.year,
            'month': obj.month,
            'day': obj.day,
            'hour': obj.hour,
            'minute': obj.minute,
            'second': obj.second,
            'weekday': obj.weekday(),
          }
        if isinstance(obj, bson.ObjectId):
          return str(obj)
        return super().default(obj)
    self.__json_encoder = JSONEncoder
    self.__swu = sendwithus.api(
      api_key = api_key,
      json_encoder = self.__json_encoder)
    self.__load_templates()

  def __load_templates(self):
    self.__templates = dict(
      (t['name'], t)
      for t in json.loads(self.__swu.templates().content.decode()))

  def __execute(self, batch):
    trace('%s: execute batch' % self)
    r = batch.execute()
    if r.status_code != 200:
      with err('%s: send with us status code: %s' % (self, r.status_code)):
        try:
          err('%s: send with us status code: %s' % (self, r.json()))
        except Exception as e:
          err('%s: non-JSON response (%s): %s' % (self, e, r.text))
      raise Exception('%s: request failed' % self)
    return r

  def __template(self, name):
    if name not in self.__templates:
      self.__load_templates()
    if name not in self.__templates:
      raise Exception('no such template: %s' % name)
    return self.__templates[name]['id']

  def send_one(self,
               template,
               recipient_email,
               recipient_name = None,
               sender_email = None,
               sender_name = None,
               reply_to = None,
               variables = None,
               ):
    return self.__send_one(self.__template(template),
                           recipient_email,
                           recipient_name,
                           sender_email,
                           sender_name,
                           reply_to,
                           variables,
                           self.__swu,
                         )

  def __send_one(self,
                 template,
                 recipient_email,
                 recipient_name,
                 sender_email,
                 sender_name,
                 reply_to,
                 variables,
                 swu,
               ):
    sender = None
    if any(x is not None for x in (sender_email, sender_name)):
      sender = {}
      if sender_email is not None:
        sender['address'] = sender_email
      if sender_name is not None:
        sender['name'] = sender_name
      if reply_to is not None:
        sender['reply_to'] = reply_to
    recipient = {
      'address': recipient_email,
    }
    if recipient_name is not None:
      recipient['name'] = recipient_name
    swu.send(
      email_id = template,
      recipient = recipient,
      sender = sender,
      email_data = variables,
    )

  def send_template(self, template, recipients):
    template = self.__template(template)
    swu = self.__swu.start_batch()
    for recipient in recipients:
      email = recipient['email']
      with trace('%s: send %s to %s' % (self, template, email)):
        dump('variables: %s' % json.dumps(recipient['vars'],
                                          cls = self.__json_encoder))
        sender_name = None
        sender_email = None
        reply_to = None
        if recipient.get('sender') is not None:
          sender_name = recipient['sender'].get('fullname')
          sender_email = recipient['sender'].get('email')
          reply_to = recipient['sender'].get('reply-to')
        self.__send_one(
          template = template,
          recipient_email = email,
          recipient_name = recipient['name'],
          sender_name = sender_name,
          sender_email = sender_email,
          reply_to = reply_to,
          variables = recipient['vars'],
          swu = swu)
      if swu.command_length() >= 100:
        dump('%s: send batch of 100' % self)
        self.__execute(swu)
    if swu.command_length() > 0:
      dump('%s: send batch of %s' % (self, swu.command_length()))
      return self.__execute(swu)


class MandrillEmailer(Emailer):

  def __init__(self, mandrill):
    self.__mandrill = mandrill

  def send_template(self, template, recipients):
    # Stupid mandrill makes it impossible to send the same template
    # twice to the same user with different merge vars. Work around.
    leftover = []
    marks = set()
    i = 0
    while i < len(recipients):
      email = recipients[i]['email']
      if email not in marks:
        marks.add(email)
        i += 1
      else:
        leftover.append(recipients[i])
        del recipients[i]
    message = {
      'to': [
        {
          'email': recipient['email'],
          'name': recipient['name'],
          'type': 'to',
        }
        for recipient in recipients
      ],
      'merge_vars': [
        {
          'rcpt': recipient['email'],
          'vars': [
            {
              'name': name,
              'content': str(content),
            }
            for name, content in recipient['vars'].items()
          ]
        }
        for recipient in recipients
      ],
    }
    res = self.__mandrill.messages.send_template(
      template_name = template,
      template_content = {},
      message = message,
      async = True)
    if leftover:
      res += self.send_template(template, leftover)
    return res


def avatar(i, meta):
  return '%s/user/%s/avatar' % (meta, i)

def user_vars(user, meta):
  res = {
    'avatar': avatar(user['_id'], meta),
    'email': user.get('email'),
    'fullname': user.get('fullname'),
    'id': str(user['_id']),
    'os': user.get('os', []),
  }
  if 'devices' in user:
    res['devices'] = user['devices']
  return res

def transaction_vars(transaction, user, meta):
  sender = transaction['sender_id'] == user['_id']
  verb = 'to' if sender else 'from'
  me =   'recipient' if not sender else 'sender'
  peer = 'recipient' if     sender else 'sender'
  def device_id(v):
    '''Change empty device ids to None'''
    if not v:
      return None
    else:
      return v
  return {
    'id': str(transaction['_id']),
    'status': transaction['status'],
    'files': transaction['files'],
    'key': key('/transactions/%s' % transaction['_id']),
    'message': transaction['message'],
    # 'device': {
    #   'name': transaction[''],
    #   },
    'device': device_id(transaction['%s_device_id' % me]),
    'peer':
    {
      'fullname': transaction['%s_fullname' % peer],
      'id': transaction['%s_id' % peer],
      'avatar': avatar(transaction['%s_id' % peer], meta),
      'device': device_id(transaction['%s_device_id' % peer]),
    },
    'size': transaction['total_size'],
    'verb': verb,
  }
