#!/usr/bin/env python3

from utils import Meta

with Meta() as meta:
  email = 'foobar@infinit.io'
  fullname = 'jean louis'

  res = meta.post('user/invite', {"email": email, "admin_token": ""})
  assert res['success']
  assert meta.database.invitations.count() == 1
  invitation = meta.database.invitations.find_one()

  assert invitation['source'] == 'infinit'
  assert invitation['email'] == email

  res = meta.post('user/register',
                  {
                    'email': email,
                    'password': 'o' * 64,
                    'activation_code': invitation['code'],
                    'fullname': fullname,
                    })
  assert res['success']

  assert meta.database.users.count() == 1
  user = meta.database.users.find_one()
  assert user['email'] == email
  assert user['fullname'] == fullname

  assert meta.database.invitations.count() == 1
  invitation = meta.database.invitations.find_one()
