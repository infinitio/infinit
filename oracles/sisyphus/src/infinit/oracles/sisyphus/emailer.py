import elle.log

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.emailer'

class SendWithUsEmailer:

  def __init__(self, send_with_us):
    self.__swu = send_with_us

  def send_template(self, template, recipients):
    for recipient in recipients:
      email = recipient['email']
      with elle.log.trace(
          '%s: send %s to %s' % (self, template, email)):
        elle.log.dump('variables: %r' % recipient['vars'])
        r = self.__swu.send(
          email_id = template,
          recipient = {
            'address': email,
            'name': recipient['name']
          },
          email_data = recipient['vars'],
        )
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
