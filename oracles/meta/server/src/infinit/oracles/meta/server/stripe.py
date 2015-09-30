import stripe

import elle.log

ELLE_LOG_COMPONENT = 'infinit.oracles.meta.server.Stripe'

class Stripe:
  def __init__(self,
               meta):
    self.__meta = meta
    self.__in_with = 0

  def __enter__(self):
    self.__in_with += 1

  def __exit__(self, type, value, tb):
    assert self.__in_with > 0
    self.__in_with -= 1
    if value is not None:
      try:
        raise value
      # Handle each exception case separately, even though the behaviour is the
      # same for now. Explicit is better than implicit.
      except (stripe.error.InvalidRequestError,
              stripe.error.AuthenticationError,
              stripe.error.APIConnectionError,
              stripe.error.StripeError) as e:
        # Invalid parameters were supplied to Stripe's API
        import traceback
        elle.log.warn('Unable to perform action (%s): %s' % (e.args[0], ''.join(traceback.format_tb(tb))))
        return self.__meta.unavailable({
          'error': 'stripe_issue',
          'reason': e.args[0]
        })
      except BaseException:
        pass

  def __stripe_date_dict(self, before = None, after = None):
    """
    Return a generic stripe date dictionary.
    By default this will be for the last year.
    """
    if after is None:
      import datetime
      def one_year_ago(from_date):
        try:
          return from_date.replace(year = from_date.year - 1)
        except:
          # Handle 29/02
          return from_date.replace(
            year = from_date.year - 1, month = 2, day = 28)
      after = one_year_ago(self.__meta.now)
    date_dict = {'gte': int(after.timestamp())}
    if before:
      date_dict.update({'lt': int(before.timestamp())})
    return date_dict

  def fetch_or_create_stripe_customer(self, user):
    if self.__meta.stripe_api_key is None:
      return None
    assert self.__in_with >= 0
    if 'stripe_id' in user:
      return self.fetch_customer(user)
    else:
      customer = stripe.Customer.create(
        email = user['email'],
        api_key = self.__meta.stripe_api_key,
      )
      self.__meta.database.users.update(
        {'_id': user['_id']},
        {'$set': {'stripe_id': customer.id}})
      return customer

  def fetch_customer(self, user):
    if self.__meta.stripe_api_key is None:
      return None
    assert self.__in_with >= 0
    return stripe.Customer.retrieve(
      user['stripe_id'],
      expand = ['subscriptions'],
      api_key = self.__meta.stripe_api_key,
    )

  def update_customer_email(self, customer, email):
    if self.__meta.stripe_api_key is None:
      return
    assert self.__in_with >= 0
    customer.email = email
    customer.save()


  def subscription(self, customer):
    # We do not want multiple plans to be active at the same
    # time, so a customer can only have at most one subscription
    # (ideally, exactly one subscription, which would be 'basic'
    # for non paying customers)
    assert self.__in_with >= 0
    if customer.subscriptions.total_count > 0:
      sub = customer.subscriptions.data[0]
    else:
      sub = None
    return sub

  def __paginate(self, stripe_call, **kwargs):
    if self.__meta.stripe_api_key is None:
      return []
    res = []
    kwargs.update({'limit': 100, 'api_key': self.__meta.stripe_api_key})
    while True:
      response = stripe_call(**kwargs)
      res.extend(response.get('data', []))
      if response['has_more']:
        kwargs.update({'ending_before': res[-1]['id']})
      else:
        break
    return res

  def charges(self, customer, before = None, after = None):
    """
    Returns a customer's charges.
    By default this returns results for the last year.
    """
    date_dict = self.__stripe_date_dict(before, after)
    try:
      charges = self.__paginate(stripe.Charge.all,
                                customer = customer,
                                created = date_dict)
    except stripe.error.StripeError as e:
      elle.log.err('error fetching charges: %s' % e)
      return []
    return charges

  def charge(self, charge_id: str):
    """
    Fetch a single charge.
    """
    if self.__meta.stripe_api_key is None:
      return None
    try:
      return stripe.Charge.retrieve(charge_id,
                                    api_key = self.__meta.stripe_api_key)
    except stripe.error.StripeError as e:
      elle.log.err('error fetching charge: %s' % e)
      return None

  def __fetch_invoices(self, customer, before, after, paid_only = False):
    date_dict = self.__stripe_date_dict(before, after)
    try:
      invoices = self.__paginate(stripe.Invoice.all,
                                 customer = customer,
                                 date = date_dict)
    except stripe.error.StripeError as e:
      elle.log.err('error fetching invoices: %s' % e)
      return []
    if not paid_only:
      return invoices
    res = []
    for i in invoices:
      if i['paid']:
        res.append(i)
    return res

  def invoice(self, invoice_id: str):
    """
    Fetch a single invoice.
    """
    if self.__meta.stripe_api_key is None:
      return None
    try:
      return stripe.Invoice.retrieve(invoice_id,
                                     api_key = self.__meta.stripe_api_key)
    except stripe.error.StripeError as e:
      elle.log.err('error fetching invoice: %s' % e)
      return None

  def invoices(self, customer, before = None, after = None):
    """
    Returns a customer's invoices.
    By default this returns results for the last year.
    """
    return self.__fetch_invoices(customer, before, after, paid_only = False)

  def receipts(self, customer, before = None, after = None):
    """
    Return a customer's paid invoices.
    By default this returns results for the last year.
    """
    return self.__fetch_invoices(customer, before, after, paid_only = True)

  def set_plan(self, customer, subscription, plan, coupon):
    elle.log.trace(
      'set plan (customer: %s, subscription: %s, plan: %s)'
      % (customer, subscription, plan))
    assert self.__in_with >= 0
    if subscription is None:
      elle.log.trace('create subscription')
      subscription = customer.subscriptions.create(
        plan = plan, coupon = coupon)
    else:
      elle.log.debug('update subscription')
      subscription.plan = plan
    return subscription

  def remove_plan(self, subscription, at_period_end = True):
    subscription.delete(at_period_end = at_period_end)
    subscription = None
    return subscription

  def update_subscription(self, customer, plan, coupon, at_period_end = False):
    elle.log.trace('update subscription (customer: %s, plan: %s, coupon: %s)'
                   % (customer, plan, coupon))
    subscription = self.subscription(customer)
    coupon_used = False
    if plan is not None:
      subscription = self.set_plan(customer, subscription, plan, coupon = coupon)
      if coupon is not None:
        coupon_used = True
    else:
      if coupon is None:
        if subscription is not None:
          subscription = self.remove_plan(subscription, at_period_end)
    if coupon is not None:
      if subscription is None:
        self.__meta.bad_request({
          'error': 'invalid_plan_coupon',
          'reason': 'cannot use a coupon on basic plan',
        })
      elle.log.debug('set coupon (%s)' % coupon)
      if not coupon_used:
        subscription.coupon = coupon
    if subscription:
      elle.log.debug('save subscription: %s' % subscription)
      subscription = subscription.save()
    return subscription
