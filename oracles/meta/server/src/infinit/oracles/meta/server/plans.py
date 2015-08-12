# This is
basic = {
  "name": "basic",
  "quotas": {
    "p2p": {
      "size_limit": int(1e10),
    },
    "links": {
      "bonuses": {
        "referrer": int(1e9),
        "referree": int(5e8),
        "facebook_linked": int(3e8),
        "social_post": int(5e8),
      },
      "default_storage": int(1e9),
    },
    "send_to_self": {
      "bonuses": {
        "referrer": 2,
        "social_post": 1,
        "facebook_linked": 1
      },
      "default_quota": 5
    }
  }
}

plus = {
  "name": "plus",
  "quotas": {
    "p2p": {
      "size_limit": None
    },
    "links": {
      "bonuses": {
        "referrer": int(1e9),
        "referree": int(5e8),
        "facebook_linked": int(3e8),
        "social_post": int(5e8),
      },
      "default_storage": int(5e9),
    },
    "send_to_self": {
      "bonuses": {
        "referrer": 2,
        "social_post": 1,
        "facebook_linked": 1
      },
      "default_quota": None
    }
  }
}

premium = {
  "name": "premium",
  "quotas": {
    "p2p": {
      "size_limit": None
    },
    "links": {
      "bonuses": {
        "referrer": int(1e9),
        "referree": int(5e8),
        "facebook_linked": int(3e8),
        "social_post": int(5e8),
      },
      "default_storage": int(1e11),
    },
    "send_to_self": {
      "bonuses": {
        "referrer": 2,
        "social_post": 1,
        "facebook_linked": 1
      },
      "default_quota": None
    }
  }
}


# Make sure plans a well formatted.

# Explore sub dictionnaries.
def explore(input, d = {}):
  if not '.' in input:
    assert input in d.keys()
  else:
    p = input.split('.')
    explore('.'.join(p[1:]), d[p[0]])

def check_field_existence(name):
  for plan in [basic, plus, premium]:
    explore(name, plan)

def check_failure(field):
  try:
    check_field_existence(field)
    raise BaseException('test failed')
  except AssertionError as e:
    pass

check_failure('unknown_field')
check_failure('quotas.links.unknown_field')

check_field_existence('quotas.p2p.size_limit')
check_field_existence('quotas.links.bonuses.referrer')
check_field_existence('quotas.send_to_self.bonuses.referrer')
