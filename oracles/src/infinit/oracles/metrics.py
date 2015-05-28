import elle.log
import json
import requests

from infinit.oracles.json import jsonify_value

ELLE_LOG_COMPONENT = 'infinit.oracles.metrics'

def slice(c, size):
  count = len(c)
  for i in range(0, count, size):
    yield c[i:min(i + size, count)]

class Metrics:

  def __init__(self,
               url = 'http://metrics.9.0.api.production.infinit.io/collections/'):
    self.__url = url

  def send(self, metrics, collection, user_agent = None):
    url = self.__url + collection
    headers = {'content-type': 'application/json'}
    if user_agent != None:
      headers['user-agent'] = user_agent
    for metrics in slice(metrics, 100):
      with elle.log.trace(
          '%s: send %s metrics to %s' % (self, len(metrics), url)):
        elle.log.dump('%s: metrics: %s' % (self, metrics))
        res = requests.post(
          url,
          headers = headers,
          data = json.dumps({'events': metrics},
                            default = jsonify_value),
        )
        elle.log.debug('metrics response: %s' % res)

  def __str__(self):
    return 'Metrics Reporter(%s...)' % self.__url
