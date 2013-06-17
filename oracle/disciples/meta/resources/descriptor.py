# -*- encoding: utf-8 -*-

from meta.page import Page, requireLoggedIn
from meta import database, conf
from meta import error
from meta import regexp

import metalib

import web

class Visibility:
    PUBLIC = 0
    PRIVATE = 1

class _Page(Page):
    """Common tools for network calls."""

    # Smart getter for network in database.
    @requireLoggedIn
    def network(self, _id):
        network = database.networks().find_one({'_id': _id})
        if not network:
            return self.forbidden("Couldn't find any network with this id")
        if len(network) == 2: # Only contains _id and owner.
            raise web.ok(data = self.error(error.NETWORK_NOT_FOUND,
                                           "Network cleared"))
        # If visibility is private, only owner and users can access it.
        if network.get('visibility', Visibility.PRIVATE) == Visibility.PRIVATE \
                and network['owner'] != self.user['_id'] \
                and self.user['_id'] not in network['users']:
            return self.forbidden("This network does not belong to you")
        return network

class PublishDescriptor(_Page):
    """XXX"""

    __pattern__ = "/descriptor/publish"

    @requireLoggedIn
    def POST(self):

        desc = self.data['descriptor']
        visibility = self.data['descriptor']
        descriptor = metalib.deserialze_descriptor(desc)
        _id = descriptor['id']

        network = database.networks().find_one(
            {"$or" :
                 [
                    {'_id': _id},
                    {'name': descriptor['name'], 'owner': self.user['_id']},
                 ]
            }
        )

        if network and len(network) > 2:
            return self.error(error.NETWORK_ALREADY_EXISTS,
                              "a network with the same %s already exists" % (network['_id'] == _id and 'id' or 'name'))
        # Someone else than the user try to republish the network.
        elif network and network.get('owner') != self.user['_id']:
            return self.error(error.NETWORK_ALREADY_EXISTS)

        network = {
            '_id': _id,
            'descriptor': desc,
            'name': descriptor['name'],
            'owner': self.user['_id'],
            'owner_handle': self.user['lw_handle'],
            'visibility': Visibility.PUBLIC,
        }
        database.networks().update({"_id": _id},
                                   network,
                                   upsert = True)
        # The best should be find_and_modify.
        # database.user().find_and_modify(self.user, {'$push': {'owned_networks': _id}})
        self.user.setdefault('owned_networks', []).append(_id)
        database.users().save(self.user)

        return self.success({'_id': _id})

class UnpublishDescriptor(_Page):
    """XXX"""

    __pattern__ = "/descriptor/unpublish"

    __validator__ = (
        ('_id', regexp.DescriptorValidator)
    )

    @requireLoggedIn
    def POST(self):
        self.validate()

        _id = self.data['id']
        network = self.network(_id)

        # Only the owner can unpublish a publish.
        if network['owner'] != self.user['_id']:
            return self.error(error.NETWORK_DOESNT_BELONG_TO_YOU)

        # Instead of deleting a network, we just reset it. With that we can
        # can differenciate deleted network id from new id ir order to create
        # smarter behavior than: "Network doesn't exist.".
        # We keep tracking the _id and the owner.
        database.networks().update(network, {key: network[key] for key in ['owner', '_id',] })

        # The best should be find_and_modify.
        # database.user().find_and_modify(self.user, {'$pull': {'owned_networks': _id}})
        assert 'owned_networks' in self.user
        self.user['owned_networks'].remove(_id)
        database.users().save(self.user)

        return self.success({'_id': _id})

class LookupDescriptor(_Page):
    """
    Get the descriptor id from owner and name.
    """

    __pattern__ = "/descriptor/lookup"

    def POST(self):
        handle = self.data['owner'].lower()
        name = self.data['name']

        network = database.networks().find_one(
            {
                'owner_handle': handle,
                'name': name,
                'visibility': Visibility.PUBLIC
            },
            fields=['_id'],
        )

        if network is None:
            return self.error(error.NETWORK_NOT_FOUND)

        return self.success({"_id": network['_id']})

class GetDescriptor(_Page):
    """
    Get the descriptor from _id.
    """

    __pattern__ = "/descriptor/get"

    @requireLoggedIn
    def POST(self):
        _id = self.data['id']
        network = self.network(_id)
        return self.success({ key: network[key] for key in ['descriptor', '_id', 'name']})

class NetworkFilter:
    ALL_ = 0
    MINE = 1
    OTHER = 2

def filtred_list(user, filter_):
  if filter_ == NetworkFilter.ALL_:
      list_ = user.setdefault('owned_networks', []) + \
        user.setdefault('networks', [])
  elif filter_ == NetworkFilter.MINE:
    list_ = user.setdefault('owned_networks', [])
  elif filter_ == NetworkFilter.OTHER:
    list_ = user.setdefault('networks', [])
  else:
    return self.error(error.UNKNOWN) # Shoul be error.UNKNOWN_FILTER
  return list_

class ListDescriptor(_Page):
    """
    Return the list of descriptor ids according to the given filter.

    """

    __pattern__ = "/descriptor/list"

    @requireLoggedIn
    def POST(self):
        return self.success(
            { "descriptors" : filtred_list(self.user, int(self.data['filter'])) }
        )

class AllDescriptor(_Page):
    """
    Return the list of descriptors according to the given filter.

    """

    __pattern__ = "/descriptor/all"

    @requireLoggedIn
    def POST(self):

        return self.success(
            { "descriptors" :  list(database.networks().find({"descriptor": {"$exists": True},
                                                              "_id": {"$in", filtred_list(self.user, int(self.data['filter']))}},
                                                             limit=10)) }
        )
