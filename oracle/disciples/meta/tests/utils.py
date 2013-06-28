#!/usr/bin/env python3

import hashlib
import pythia

def hash_password(password):
    return hashlib.sha256(password.encode()).hexdigest()

def create_client(meta):
    session = {}
    print(meta.url)
    client = pythia.Client(session = session,
                           server = meta.url)

    email = 'testkaka@infinit.io'
    password = 'kittens'
    password_hash = hash_password(password)
    fullname = 'Pif Pif'
    activation_code = 'bitebite'

    res = pythia.Admin(server = meta.url).post('/user/register',
                                               {'email': email,
                                                'fullname': fullname,
                                                'password': password_hash,
                                                'activation_code': activation_code,
                                            })
    if not res['success']:
        raise Exception("Cannot register: " + res['error'])
    res = client.post('/user/login', {'email': email,
                                      'password': password_hash,
    })

    if not res['success']:
        print(res)
        raise Exception("Cannot login!")
    session['token'] = res['token']
    print("Got token:", res['token'])
    return client
