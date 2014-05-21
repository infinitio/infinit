import infinit.oracles.meta.server

application = infinit.oracles.meta.server.Meta(debug = True,
                                               aws_region = 'us-east-1',
                                               aws_buffer_bucket = 'us-east-1-buffer-dev-infinit-io',
                                               aws_link_bucket = 'us-east-1-links-dev-infinit-io')
