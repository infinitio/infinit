import elle.log

import json

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.emailer'

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


class SendWithUsEmailer:

  def __init__(self, api_key):
    import sendwithus
    self.__swu = sendwithus.api(
      api_key = api_key,
      json_encoder = JSONEncoder)
    self.__load_templates()

  def __load_templates(self):
    self.__templates = dict(
      (t['name'], t)
      for t in json.loads(self.__swu.templates().content.decode()))

  def send_template(self, template, recipients):
    if template not in self.__templates:
      self.__load_templates()
    template = self.__templates[template]['id']

    swu = self.__swu.start_batch()
    for recipient in recipients:
      email = recipient['email']
      if recipient['sender'] is not None:
        sender = {}
        if 'fullname' in recipient['sender']:
          sender['name'] = recipient['sender']['fullname']
        if 'email' in recipient['sender']:
          sender['address'] = recipient['sender']['email']
      else:
        sender = None
      with elle.log.trace(
          '%s: send %s to %s%s' % (
            self, template, email,
            ' %s' % sender if sender is not None else '')):
        elle.log.dump('variables: %s' % json.dumps(recipient['vars'],
                                                   cls = JSONEncoder))
        r = swu.send(
          email_id = template,
          recipient = {
            'address': email,
            'name': recipient['name']
          },
          sender = sender,
          email_data = recipient['vars'],
        )
    r = swu.execute()
    assert r.status_code == 200

class MandrillEmailer:

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
