#!/usr/bin/env python3

from utils import Meta, HTTPException

def throws(f, status):
  assert isinstance(status, int)
  try:
    f()
    assert status == 200
  except HTTPException as e:
    if status != 200:
      assert isinstance(e.status, int)
      assert e.status == status

with Meta() as meta:

  throws(lambda: meta.post('user/login', {}), 400)
  throws(lambda: meta.get('user/logout'), 200)
  throws(lambda: meta.post('user/register', {}), 400)
  throws(lambda: meta.post('user/search', {}), 400)
  throws(lambda: meta.get('user/email@email.com/view'), 200)
  throws(lambda: meta.post('user/from_public_key', {}), 400)
  throws(lambda: meta.get('user/swaggers'), 403)
  throws(lambda: meta.post('user/remove_swagger', {}), 403)
  throws(lambda: meta.post('user/favorite', {}), 403)
  throws(lambda: meta.post('user/unfavorite', {}), 403)
  throws(lambda: meta.post('user/edit', {}), 403)
  throws(lambda: meta.post('user/invite', {}), 400)
  throws(lambda: meta.get('user/invited'), 200)
  throws(lambda: meta.get('self'), 200)
  throws(lambda: meta.get('minimumself'), 200)
  throws(lambda: meta.get('user/remaining_invitations'), 200)
  # throws(lambda: meta.get('user/5241b9d226b9f10fff8be391/avatar'), 200)
  # throws(lambda: meta.post('user/5241b9d226b9f10fff8be391/avatar', {}), 200)
  throws(lambda: meta.post('debug', {}), 403)
  throws(lambda: meta.post('user/add_swagger', {}), 400)
  throws(lambda: meta.post('user/connect', {}), 400)
  throws(lambda: meta.post('user/disconnect', {}), 400)
