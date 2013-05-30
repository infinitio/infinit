# -*- encoding: utf-8 -*-

from meta import conf
from meta.page import Page
import metalib

class Sign(Page):
  __pattern__ = "/authority/sign"

  _mendatory_fields = [
    ('hash', str),
  ]

  def POST(self):
    signature = metalib.sign(self.data['hash'],
                             conf.INFINIT_AUTHORITY_PATH,
                             conf.INFINIT_AUTHORITY_PASSWORD,
                             )

    assert isinstance(signature, str)
    return self.success({
        'signature': signature
    })


class Verify(Page):
  __pattern__ = "/authority/verify"

  _mendatory_fields = [
    ('signature', str),
    ('hash', str),
  ]

  def POST(self):
    verified = metalib.verify(self.data['signature'],
                              self.data['hash'],
                              conf.INFINIT_AUTHORITY_PATH,
                              conf.INFINIT_AUTHORITY_PASSWORD,
                              )

    assert isinstance(verified, bool)
    return self.success({
        'verified': verified
    })
