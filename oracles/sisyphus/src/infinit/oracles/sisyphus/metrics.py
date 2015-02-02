import elle.log
import json
import requests

ELLE_LOG_COMPONENT = 'infinit.oracles.sisyphus.metrics'

def slice(c, size):
  count = len(c)
  for i in range(0, count, size):
    yield c[i:min(i + size, count)]

class Metrics:

  def __init__(self):
    pass

  def send(self, metrics):
    for metrics in slice(metrics, 100):
      with elle.log.trace(
          '%s: send %s metrics' % (self, len(metrics))):
        url = 'http://metrics.9.0.api.production.infinit.io/collections/users'
        elle.log.dump('%s: metrics: %s' % (self, metrics))
        res = requests.post(
          url,
          headers = {'content-type': 'application/json'},
          data = json.dumps({'events': metrics}),
        )
        elle.log.debug('metrics response: %s' % res)
