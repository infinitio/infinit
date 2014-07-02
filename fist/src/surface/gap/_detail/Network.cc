#include <surface/gap/State.hh>

namespace surface
{
  namespace gap
  {
   //  State::NetworkNotFoundException::NetworkNotFoundException(uint32_t id):
   //    Exception{gap_network_not_found, elle::sprintf("unknown network %s", id)}
   //  {}

   //  State::NetworkNotFoundException::NetworkNotFoundException(std::string const& id):
   //    Exception{gap_network_not_found, elle::sprintf("unknown network %s", id)}
   // {}

   //  static
   //  uint32_t
   //  generate_id()
   //  {
   //    static uint32_t id = 0;
   //    return ++id;
   //  }

   //  State::Network
   //  State::network_sync(State::Network const& network)
   //  {
   //    ELLE_TRACE_SCOPE("%s: network response: %s", *this, network);

   //    uint32_t id = 0;
   //    try
   //    {
   //      // If the network already in the cache, we keep his index and replace the
   //      // data by the new fetched one.
   //      id = this->_network_indexes.at(network.id);
   //      ELLE_ASSERT_NEQ(id, 0u);
   //      this->_networks.at(id) = std::move(network);
   //    }
   //    catch (std::out_of_range const&)
   //    {
   //      id = generate_id();
   //      this->_network_indexes[network.id] = id;
   //      this->_networks.emplace(id, std::move(network));
   //    }

   //    ELLE_ASSERT_NEQ(id, 0u);
   //    return this->_networks.at(id);
   //  }

   //  State::Network
   //  State::network_sync(std::string const& id)
   //  {
   //    ELLE_TRACE_SCOPE("%s: sync network from object id %s", *this, id);

   //    return this->network_sync(this->meta().network(id));
   //  }

   //  State::Network
   //  State::network(std::string const& network_id,
   //              bool merge)
   //  {
   //    ELLE_TRACE_SCOPE("%s: network from object id %s", *this, network_id);

   //    try
   //    {
   //      uint32_t id = this->_network_indexes.at(network_id);
   //      return this->network(id);
   //    }
   //    catch (std::out_of_range const&)
   //    {
   //      if (merge)
   //      {
   //        ELLE_DEBUG("%s: network not found, merging it", *this);
   //        return this->network_sync(network_id);
   //      }

   //      throw State::NetworkNotFoundException(network_id);
   //    }
   //  }

   //  State::Network
   //  State::network(uint32_t id)
   //  {
   //    ELLE_TRACE_SCOPE("%s: sync network from id %s", *this, id);

   //    try
   //    {
   //      return this->_networks.at(id);
   //    }
   //    catch (std::out_of_range const&)
   //    {
   //      throw State::NetworkNotFoundException(id);
   //    }
   //  }

   //  State::Network
   //  State::network(std::function<bool (State::NetworkPair const&)> const& func)
   //  {
   //    ELLE_TRACE_SCOPE("%s: find network", *this);

   //    typedef NetworkMap::const_iterator It;

   //    It begin = this->_networks.begin();
   //    It end = this->_networks.end();

   //    It res = std::find_if(begin, end, func);

   //    if (res != end)
   //      return res->second;
   //    else
   //      throw State::NetworkNotFoundException("from find");
   //  }

   //  uint32_t
   //  State::network_create(std::string const& name,
   //                        bool auto_add = true)
   //  {
   //    this->meta().create_network(name);
   //  }

   //  void
   //  State::network_prepare(std::string const& network_id)
   //  {
   //  }

   //  std::string
   //  State::network_delete(std::string const& name,
   //                        bool force)
   //  {
   //  }

   //  void
   //  State::network_add_user(std::string const& network_id,
   //                          std::string const& inviter_id,
   //                          std::string const& user_id,
   //                          std::string const& identity)
   //  {
   //  }

   //  void
   //  State::on_network_update_notification(
   //    plasma::trophonius::NetworkUpdateNotification const& notif)
   //  {
   //  }

  }
}
