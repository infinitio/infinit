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
      user = self.sisyphus.mongo.meta.users.find_and_modify(
        query = {
          'devices.push_token': token,
        },
        update = {
          '$unset':
          {
            'devices.$.push_token': True,
          }
        },
        fields = {
          '_id': True,
        }
      )
      if self.sisyphus.metrics is not None:
        res = self.sisyphus.metrics.send(
          {
            'event': 'push token invalidated',
            'timestamp': time.time(),
            'fail_time': fail_time,
            'user': str(user['_id']),
          })

  def status(self):
    return {}
