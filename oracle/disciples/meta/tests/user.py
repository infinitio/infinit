#!/usr/bin/env python3

def run(client):
    res = client.post('/user/register', {
        'fullname': 'Mister Test friend',
        'email': "friend@test.net",
        'password': utils.hash_password('kittens')
    })

    assert res['success'] is False
    assert client.get('/user/logout')['success'] is True

if __name__ == "__main__":
    import utils
    import meta
    with meta.Meta(spawn_db = True) as meta:
        client = utils.create_client(meta)
        run(client)
