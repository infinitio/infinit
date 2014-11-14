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
