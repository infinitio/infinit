#!/usr/bin/env python3

def run(client):
    # Testing devices
    if not client.get('/devices')['devices']:
        res = client.post('/device/create', {
            'name': "New device",
        })
        print(res)
        assert res['success'] is True
        print("added device", res['created_device_id'])

    devices = client.get('/devices')['devices']
    assert len(devices) > 0
    print("User devices:", devices)
    device = client.get('/device/{}/view'.format(devices[0]))
    print("Got device", device)

    device['name'] = "THIS IS A NEW NAME"
    new_device = client.post('/device/create', device)

    device = client.get('/device/{}/view'.format(new_device["created_device_id"]))
    print("Got updated device", device)
    assert device['name'] == "THIS IS A NEW NAME"

    res = client.post('/device/delete', {"_id": devices[0]})
    assert res['success'] is True
    assert res["deleted_device_id"] == devices[0]

import utils
with utils.Servers(trophonius = False, apertus = False) \
     as (meta, troph, apertus):
    client = utils.create_client(meta)
    run(client)
