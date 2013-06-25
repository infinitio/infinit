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
    DESCRIPTOR_MAX_SIZE = 1048576 # 1M

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
        self.check_visiblity(network)
        return network

    @requireLoggedIn
    def network_by_name(self, network_name, owner_lw_handle = None):

        if owner_lw_handle is None or owner_lw_handle == "":
            user = self.user
        else:
            user = database.users().find_one({"lw_handle": owner_lw_handle})
            if user is None:
                return self.error(error.UNKNOWN_USER)

        req = {
            'owner': user['_id'],
            'name': network_name,
        }

        # The user can lookup all networks. The other can only
        # lookup the public ones.
        if self.user['lw_handle'] != owner_lw_handle:
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

    __mandatory_fields__ = [
        ('descriptor', basestring),
        ('network_name', basestring),
    ]

    __pattern__ = "/descriptor/publish"

    @requireLoggedIn
    def POST(self):

        descriptor = self.data['descriptor']

        if len(descriptor) > self.DESCRIPTOR_MAX_SIZE:
            return self.error(error.UNKNOWN_ERROR, "Descriptor is too big.")

        name = self.data['network_name']
        visibility = Visibility.PUBLIC

        network = database.networks().find_one(
            { 'name': name, 'owner': self.user['_id'] },
        )

        network = {
            'descriptor': descriptor,
            'name': name,
            'owner': self.user['_id'],
            'visibility': Visibility.PUBLIC,
        }
        database.networks().save(network, upsert = True)

        # The best should be find_and_modify.
        # database.user().find_and_modify(self.user, {'$push': {'owned_networks': _id}})
        self.user.setdefault('owned_networks', []).append(name)
        database.users().save(self.user)

        return self.success({'name': name})

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

    __mandatory_fields__ = [
        ('network_name', basestring),
    ]

    __pattern__ = "/descriptor/unpublish"

    @requireLoggedIn
    def POST(self):
        network_name = network['network_name']

        network = self.network_by_name(network_name = network_name)

        # Only the owner can unpublish a published descriptor.
        if network['owner'] != self.user['_id']:
            return self.error(error.NETWORK_DOESNT_BELONG_TO_YOU)

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
        self.user['owned_networks'].remove(name)
        database.users().save(self.user)

        return self.success({'name': name})

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

    __mandatory_fields__ = [
        ('owner_handle', basestring),
        ('network_name', basestring),
    ]

    __pattern__ = "/descriptor/get"

    @requireLoggedIn
    def POST(self):
        network = self.network_by_name(network_name = self.data['network_name'],
                                       owner_handle = self.data['owner_handle'])

        return self.success({ key: network[key] for key in ['descriptor', 'name'] })

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
