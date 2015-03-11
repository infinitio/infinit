import apns
import elle

from .. import Boulder

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.boulders.apns'

class APNSUnregister(Boulder):

  def __init__(self, sisyphus, certificate, production):
    super().__init__(sisyphus)
    self.__cert = certificate
    self.__apns = apns.APNs(
      use_sandbox = not production,
      cert_file = self.__cert)

  def run(self):
    for token, fail_time in self.__apns.feedback_server.items():
      token = token.decode('utf-8')
      elle.log.debug('%s: drop invalid token %s' % (self, token))
      self.sisyphus.mongo.meta.users.update(
        {
          'devices.push_token': token,
        },
        {
          '$unset':
          {
            'devices.$.push_token': True,
          }
        }
      )

  def status(self):
    return {}
