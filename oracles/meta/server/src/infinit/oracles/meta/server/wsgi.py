import infinit.oracles.meta.server
import infinit.oracles.emailer
import os

os.environ['META_LOG_SYSTEM'] = 'meta'
os.environ['ELLE_LOG_LEVEL'] = 'infinit.oracles.meta.*:DEBUG'

swu_key = 'live_7e775f6f0e1404802a5fbbc0fcfa9c238b065c49'
emailer = infinit.oracles.emailer.SendWithUsEmailer(api_key = swu_key)
stripe_key = 'sk_live_gplnR4VlHoO843kfacKkk591'

application = infinit.oracles.meta.server.Meta(
  mongo_replica_set = ['mongo-0', 'mongo-1', 'mongo-2',],
  aws_region = 'us-east-1',
  aws_buffer_bucket = 'us-east-1-buffer-infinit-io',
  aws_link_bucket = 'us-east-1-links-infinit-io',
  emailer = emailer,
  stripe_api_key = stripe_key,
  production = True,
)
