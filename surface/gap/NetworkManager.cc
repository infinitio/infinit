#include "NetworkManager.hh"

#include <elle/assert.hh>
#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.NetworkManager");

namespace surface
{
  namespace gap
  {
    namespace json = elle::format::json;

    NetworkManager::NetworkManager(surface::gap::NotificationManager& notification_manager,
                                   plasma::meta::Client& meta):
      Notifiable(notification_manager),
      _meta(meta)
    {
      ELLE_TRACE_METHOD("");

      this->_notification_manager.network_update_callback(
        [&] (NetworkUpdateNotification const &n) -> void
        {
          this->_on_network_update(n);
        });
    }

    NetworkManager::~NetworkManager()
    {
      ELLE_TRACE_METHOD("");
    }

    NetworkManager::NetworkMap const&
    NetworkManager::all()
    {
      if (this->_networks->get() != nullptr)
        return *this->_networks->get();

      this->_networks([] (NetworkMapPtr& map) {
        if (map == nullptr)
          map.reset(new NetworkMap{});
      });

      auto response = this->_meta.networks();
      for (auto const& id: response.networks)
      {
        auto network = this->_meta.network(id);
        this->_networks([&id, &network] (NetworkMapPtr& map) {
            (*map)[id] = network;
        });
      }

      return *(this->_networks->get());
    }

    std::vector<std::string>
    NetworkManager::all_ids()
    {
      ELLE_TRACE_METHOD("");

      this->all(); // Ensure creation.

      return this->_networks(
        [](NetworkMapPtr const& map)
        {
          std::vector<std::string> res{map->size()};
          for (auto const& pair: *map)
            res.emplace_back(pair.first);
          return res;
        }
      );
    }

    Network
    NetworkManager::one(std::string const& id)
    {
      ELLE_TRACE_METHOD("");

      auto it = this->all().find(id);
      if (it != this->all().end())
        return it->second;
      return this->sync(id);
    }

    Network
    NetworkManager::sync(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);

      try
      {
        auto network = this->_meta.network(id);
        return this->_networks(
          [&id, &network] (NetworkMapPtr& map) -> Network
          {
            return (*map)[id] = network;
          });
      }
      catch (elle::Exception const& e)
      {
        throw Exception{gap_network_error, e.what()};
      }
      elle::unreachable();
    }

    void
    NetworkManager::_on_network_update(NetworkUpdateNotification const& notif)
    {
      ELLE_TRACE_METHOD(notif);

      // XXX do something
    }

    /*----------.
    | Printable |
    `----------*/
    void
    NetworkManager::print(std::ostream& stream) const
    {
      stream << "NetworkManager(" << this->_meta.email() << ")";
    }
  }
}
