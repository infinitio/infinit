#!/usr/bin/env python3

from utils import Meta

def throws(f, status):
  assert isinstance(status, int)
  try:
    f()
    assert status == 200
  except Meta.Exception as e:
    if status != 200:
      assert isinstance(e.status, int)
      print(status, e.status)
      assert e.status == status

with Meta() as meta:

  throws(lambda: meta.post('user/login', {}), 400)
  throws(lambda: meta.get('user/logout'), 200)
  throws(lambda: meta.post('user/register', {}), 400)
  throws(lambda: meta.post('user/search', {}), 400)
  throws(lambda: meta.get('user/<id_or_email>/view'), 200)
  throws(lambda: meta.post('user/from_public_key', {}), 400)
  throws(lambda: meta.get('user/swaggers'), 200)
  throws(lambda: meta.post('user/remove_swagger', {}), 400)
  throws(lambda: meta.post('user/favorite', {}), 400)
  throws(lambda: meta.post('user/unfavorite', {}), 400)
  throws(lambda: meta.post('user/edit', {}), 400)
  throws(lambda: meta.post('user/invite', {}), 400)
  throws(lambda: meta.get('user/invited'), 200)
  throws(lambda: meta.get('self'), 200)
  throws(lambda: meta.get('minimumself'), 200)
  throws(lambda: meta.get('user/remaining_invitations'), 200)
  throws(lambda: meta.get('user/id/avatar'), 200)
  throws(lambda: meta.post('user/id/avatar', {}), 200)
  throws(lambda: meta.post('debug', {}), 400)
  throws(lambda: meta.post('user/add_swagger', {}), 400)
  throws(lambda: meta.post('user/connect', {}), 400)
  throws(lambda: meta.post('user/disconnect', {}), 400)
