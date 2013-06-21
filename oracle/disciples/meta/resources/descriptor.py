# -*- encoding: utf-8 -*-

from meta.page import Page, requireLoggedIn
from meta import database, conf
from meta import error
from meta import regexp

import metalib

import web

#The visibility represents
class Visibility:
    PUBLIC = 0
    PRIVATE = 1

class _Page(Page):
    """Common tools for network calls."""

    # Smart getter for network in database.
    @requireLoggedIn
    def check_visiblity(self, network):
        # If visibility is private, only owner and users can access it.
        if network.get('visibility', Visibility.PRIVATE) == Visibility.PRIVATE \
                and network['owner'] != self.user['_id'] \
                and self.user['_id'] not in network['users']:
            return self.forbidden("This network does not belong to you")

    def _network(self, network):
        if not network:
            return self.forbidden("Couldn't find any network with this id")
        if len(network) == 2: # Only contains _id and owner.
            raise web.ok(data = self.error(error.NETWORK_NOT_FOUND,
                                           "Network cleared"))
        self.check_visiblity(network)
        return network

    @requireLoggedIn
    def network_by_id(self, _id):
        return self._network(database.networks().find_one({'_id': _id}))

    @requireLoggedIn
    def network_by_name(self, owner_handle, network_name):

        if owner_handle is None or owner_handle == "":
            owner_handle = self.user['lw_handle']

        req = {
            'owner_handle': owner_handle,
            'name': network_name
        }

        # The user can lookup all networks. The other can only
        # lookup the public ones.
        if not self.user or self.user['lw_handle'] != owner_handle:
            req['visibility'] = Visibility.PUBLIC

        return self._network(database.networks().find_one(req))

class PublishDescriptor(_Page):
    """
    Store the descriptor into the database.
    It requieres to be logged in.
    The metalib deserializes the descriptor, validates the ownership.
    It also validate the descriptor signature it self.
    XXX: This means that only the local authority can be used to
    sign the descriptor.

    POST {
             'descriptor': The digest of the descriptor.
         }
         ->
         {
             '_id': The identifier of the network.
         }

    """

    __pattern__ = "/descriptor/publish"

    @requireLoggedIn
    def POST(self):

        digest = self.data['descriptor_digest']
        name = self.data['name']

        visibility = Visibility.PUBLIC
        descriptor = metalib.deserialze_descriptor(digest)
        _id = unicode(descriptor['id'])

        network = database.networks().find_one(
            {"$or" :
                 [
                    {'_id': _id},
                    {'name': name, 'owner': self.user['_id']},
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
            'descriptor': digest,
            'name': name,
            'owner': self.user['_id'],
            'owner_handle': self.user['lw_handle'],
            'visibility': Visibility.PUBLIC,
        }
        database.networks().update({"_id": _id},
                                   network,
                                   upsert = True)
        # The best should be find_and_modify.
        # database.user().find_and_modify(self.user, {'$push': {'owned_networks': _id}})
        self.user.setdefault('owned_networks', []).append((_id, name))
        database.users().save(self.user)

        return self.success({'_id': _id})

class UnpublishDescriptor(_Page):
    """
    Unstore the descriptor into the database.
    It requieres to be logged in.
    This function reset most the fields except owner and id, allowing the user
    to republish a deleted descriptor. To avoid database growing we can check
    the last update of the entry and delete it after a specific duration.

    POST {
             '_id': The identifier of the network.
         }
         ->
         {
             '_id': The identifier of the network.
         }

    """

    __pattern__ = "/descriptor/unpublish"

    __validator__ = (
        ('_id', regexp.DescriptorValidator)
    )

    @requireLoggedIn
    def POST(self):
        self.validate()

        network = self.network_by_id(self.data['id'])

        # Only the owner can unpublish a published descriptor.
        if network['owner'] != self.user['_id']:
            return self.error(error.NETWORK_DOESNT_BELONG_TO_YOU)

        _id = network['_id']
        name = network['name']

        # Instead of deleting a network, we just reset it. With that we can
        # differenciate deleted network id from new id in order to create
        # smarter behavior "The owner unpublished this descriptor" instead of
        # "Network doesn't exist.".
        # We keep tracking the _id and the owner.
        # database.networks().update(network, {key: network[key] for key in ['owner', '_id',] })
        database.networks().remove(network)

        # The best should be find_and_modify.
        # database.user().find_and_modify(self.user, {'$pull': {'owned_networks': _id}})
        assert 'owned_networks' in self.user

        # Mongo can't store tuple, which are automaticly converted into list.
        self.user['owned_networks'].remove(list((_id, name)))
        database.users().save(self.user)

        return self.success({'_id': _id})

class LookupDescriptor(_Page):
    """
    Get the descriptor id from owner and name.

    POST {
             'owner': The owner of the handle (not case sensitiv).
             'name': The name of the network to lookup.
         }
         ->
         {
             '_id': The identifier of the network.
         }

    """

    __pattern__ = "/descriptor/lookup"

    def POST(self):
        handle = self.data['owner'].lower()
        name = self.data['name']

        return self.success({"_id": self.network_by_name(handle, name)['_id']})

class GetDescriptor(_Page):
    """
    Get the descriptor from _id.


    POST {
             '_id': The network id
         }
         ->
         {
             'name': The name of the network.
             '_id': The identifier of the network.
             'descriptor': The digest of the descriptor.
         }
    """

    __pattern__ = "/descriptor/get"

    @requireLoggedIn
    def POST(self):
        if 'id' in self.data:
            network = self.network_by_id(self.data['id'])
        elif 'owner_handle' in self.data and 'network_name' in self.data:
            network = self.network_by_name(self.data['owner_handle'], self.data['network_name'])
        else:
            return self.error(error.BAD_REQUEST)
        return self.success({ key: network[key] for key in ['descriptor', '_id', 'name'] })

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

    POST {
             'filter': The filter for the list. Not used for the moment.
         }
         ->
         {
             'descriptors': The list of the network identifier matching the filter.
         }
    """

    __pattern__ = "/descriptor/list"

    @requireLoggedIn
    def POST(self):
        return self.success(
            { "descriptors" : [pair[1] for pair in filtred_list(self.user, int(self.data['filter']))] }
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
