#!/usr/bin/env python3.2
import utils

def run(client):
    # Ensure we're logged out
    assert client.get('/user/logout')['success'] is True

    def register(user):
        email = '%s@infinit.io' % user
        res = client.post('/user/register', {
            'fullname': '%s' % user,
            'email': email,
            'password': utils.hash_password('%s_password' % user),
            'activation_code': 'bitebite',
        })
        return res, client.get('/user/%s/view' % email, {})['_id']

    def login(user):
        email = '%s@infinit.io' % user
        res = client.post('/user/login', {
          'email': email,
          'password': utils.hash_password('%s_password' % user),
        })
        return res, client.get('/user/%s/view' % email, {})['_id']

    # Registration test
    res, user1 = register('user1')
    assert res['success'] is True

    # Login test
    res, user1 = login('user1')
    assert res['success'] is True

if __name__ == "__main__":
    import meta
    with meta.Meta(spawn_db = True) as meta:
        client = utils.create_client(meta)
        run(client)
