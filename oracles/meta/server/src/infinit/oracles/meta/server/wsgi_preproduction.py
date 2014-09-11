import infinit.oracles.meta.server
import os

os.environ['META_LOG_SYSTEM'] = 'meta'
os.environ['ELLE_LOG_LEVEL'] = 'infinit.oracles.meta.*:DEBUG'

application = infinit.oracles.meta.server.Meta(
  mongo_replica_set = ['0.mongo.production.infinit.io'],
  aws_region = 'us-east-1',
  aws_buffer_bucket = 'us-east-1-buffer-infinit-io',
  aws_link_bucket = 'us-east-1-links-infinit-io',
  zone = preproduction,
)
