class BuiltInPlans:

  @staticmethod
  def basic():
    return {
      'name': 'basic',
      'interval': 'mounth',
      'interval_count': 1,
      'quotas': {
        'p2p': {
          'size_limit': int(1e10),
        },
        'links': {
          'bonuses': {
            'referrer': int(1e9),
            'referree': int(5e8),
            'facebook_linked': int(3e8),
            'social_post': int(5e8),
          },
          'default_storage': int(1e9),
        },
        'send_to_self': {
          'bonuses': {
            'referrer': 2,
            'social_post': 2,
            'facebook_linked': 2,
          },
          'default_quota': 5,
        }
      }
    }

  @staticmethod
  def plus():
    return {
      'name': 'plus',
      'interval': 'mounth',
      'interval_count': 1,
      'quotas': {
        'p2p': {
          'size_limit': None,
        },
        'links': {
          'bonuses': {
            'referrer': int(1e9),
            'referree': int(5e8),
            'facebook_linked': int(3e8),
            'social_post': int(5e8),
          },
          'default_storage': int(5e9),
        },
        'send_to_self': {
          'bonuses': {
            'referrer': 2,
            'social_post': 2,
            'facebook_linked': 2,
          },
          'default_quota': None,
        }
      }
    }

  @staticmethod
  def premium():
    return {
      'name': 'premium',
      'interval': 'mounth',
      'interval_count': 1,
      'quotas': {
        'p2p': {
          'size_limit': None,
        },
        'links': {
          'bonuses': {
            'referrer': int(1e9),
            'referree': int(5e8),
            'facebook_linked': int(3e8),
            'social_post': int(5e8),
          },
          'default_storage': int(1e11),
        },
        'send_to_self': {
          'bonuses': {
            'referrer': 2,
            'social_post': 2,
            'facebook_linked': 2,
          },
          'default_quota': None,
        }
      }
    }

  @staticmethod
  def team():
    return {
      'name': 'team',
      'interval': 'mounth',
      'interval_count': 1,
      'team': True,
      'quotas': {
        'p2p': {
          'size_limit': None,
        },
        'links': {
          'default_storage': None,
          'per_user_storage': int(15e10),
        },
        'send_to_self': {
          'default_quota': None,
        }
      }
    }
