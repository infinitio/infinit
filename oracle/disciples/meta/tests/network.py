def run(client):
    # Testing networks
    if not client.get('/networks')['networks']:
        res = client.post('/network/create', {'name': "New network"})
        assert res['success'] is True
        print("added network", res['created_network_id'])

    networks = client.get('/networks')['networks']
    print("user networks", networks)
    assert len(networks) > 0

    # updating network
    network = client.get('/network/{}/view'.format(networks[0]))
    network['name'] = "THIS IS A NEW `network` NAME"
    res = client.post('/network/update', {"_id": network['_id'], "name": network['name']})
    if res['success'] is False:
        print("ERROR:", res)
        assert False

    # Test for deprecation of the users key.
    res = client.post('/network/create', {"name": "Net Stark", "users": ["idid"]})
    print(res)
    assert res['success'] is False and res['error_details'] == 'The key users is deprecated'

    # Test for deprecation of the devices key.
    res = client.post('/network/create', {"name": "Net Stark", "devices": ["devdev"]})
    print(res)
    assert res['success'] is False and res['error_details'] == 'The key devices is deprecated'

    network = client.get('/network/{}/view'.format(networks[0]))
    print(network)
    assert network['name'] == "THIS IS A NEW `network` NAME"
    print("updated network", network)
