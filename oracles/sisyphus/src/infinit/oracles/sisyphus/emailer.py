class MandrillEmailer:

  def __init__(self, mandrill):
    self.__mandrill = mandrill

  def send_template(self, template, recipients):
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
    return self.__mandrill.messages.send_template(
      template_name = template,
      template_content = {},
      message = message,
      async = True)
