import infinit.oracles.meta.server
import infinit.oracles.emailer

swu_key = 'test_26008542af128a451ffd05a954c971066219b090'
emailer = infinit.oracles.emailer.SendWithUsEmailer(api_key = swu_key)
stripe_key = 'sk_test_WtXpwiieEsemLlqrQeKK0qfI'

application = infinit.oracles.meta.server.Meta(
  aws_region = 'us-east-1',
  aws_buffer_bucket = 'us-east-1-buffer-dev-infinit-io',
  aws_invite_bucket = 'us-east-1-invite-infinit-io',
  aws_link_bucket = 'us-east-1-links-dev-infinit-io',
  emailer = emailer,
  stripe_api_key = stripe_key,
  debug = True,
)
