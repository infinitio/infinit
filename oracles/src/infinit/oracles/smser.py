import nexmo

import elle.log

ELLE_LOG_COMPONENT = 'infinit.oracles.SMSer'

class SMSer:

  @property
  def api_key(self):
    return '5e948e95'

  def __init__(self, nexmo_api_secret):
    self.__client = None
    if nexmo_api_secret:
      self.__client = nexmo.Client(key = self.api_key,
                                   secret = nexmo_api_secret)

  ## ---------------- ##
  ## Sending Messages ##
  ## ---------------- ##

  def send_message(self, destination, message):
    """
    Send a text message to a mobile number.
    destination -- number in international format as a string.
    message -- text to be sent.
    """
    elle.log.trace('send SMS to %s' % destination)
    assert len(destination) and len(message)
    if not self.__client:
      return True
    elle.log.debug('send message to %s: %s' % (destination, message))
    res = self.__client.send_message({
      'from': 'Infinit',
      'to': destination,
      'text': message
    })
    for m in res['messages']:
      if m['status'] != 0:
        elle.log.warn('Error sending SMS: %s' % res['error-text'])
        return False
    return True

  ## ------- ##
  ## Helpers ##
  ## ------- ##

  def iso_country_code_for_number(self, number):
    """
    Determine the ISO 3166-1 alpha-2 country code for a given number.
    number -- number in international format as a string.
    """
    from phonenumbers import geocoder
    parsed = phonenumbers.parse(number)
    region_codes = geocoder.region_codes_for_country_code(parsed.country_code)
    for region_code in region_codes:
      if geocoder.is_valid_number_for_region(parsed, region_code):
        return region_code
    return None

class NoopSMSer(SMSer):

  def __init__(self):
    pass

  def send_message(self, destination, message):
    elle.log.trace('would send SMS to %s: %s' % (destination, message))
