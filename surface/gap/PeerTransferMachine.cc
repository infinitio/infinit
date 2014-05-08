#include <boost/filesystem.hpp>

#include <elle/container/vector.hh>
#include <elle/container/map.hh>
#include <elle/log.hh>
#include <elle/network/Interface.hh>
#include <elle/os/environ.hh>

#include <reactor/network/exception.hh>

#include <protocol/exceptions.hh>

#include <station/src/station/Station.hh>
#include <surface/gap/PeerTransferMachine.hh>
#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/Rounds.hh>
#include <surface/gap/SendMachine.hh>
#include <surface/gap/State.hh>
#include <surface/gap/TransactionMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.TransferMachine");

namespace surface
{
  namespace gap
  {
    PeerTransferMachine::PeerTransferMachine(TransactionMachine& owner)
      : Transferer(owner)
    {}

    void
    PeerTransferMachine::_publish_interfaces()
    {
      auto& station = this->_owner.station();
      typedef std::vector<std::pair<std::string, uint16_t>> AddressContainer;
      AddressContainer addresses;
      // In order to test the fallback, we can fake our local addresses.
      // It should also work for nated network.
      if (elle::os::getenv("INFINIT_LOCAL_ADDRESS", "").length() > 0)
      {
        addresses.emplace_back(elle::os::getenv("INFINIT_LOCAL_ADDRESS"),
                               station.port());
      }
      else
      {
        auto interfaces = elle::network::Interface::get_map(
          elle::network::Interface::Filter::only_up |
          elle::network::Interface::Filter::no_loopback |
          elle::network::Interface::Filter::no_autoip
          );
        for (auto const& pair: interfaces)
          if (pair.second.ipv4_address.size() > 0 &&
              pair.second.mac_address.size() > 0)
          {
            auto const &ipv4 = pair.second.ipv4_address;
            addresses.emplace_back(ipv4, station.port());
          }
      }
      ELLE_DEBUG("addresses: %s", addresses);
      AddressContainer public_addresses;
      this->_owner.state().meta().transaction_endpoints_put(
        this->_owner.data()->id,
        this->_owner.state().passport().id(),
        addresses,
        public_addresses);
    }

    std::unique_ptr<station::Host>
    PeerTransferMachine::_connect()
    {
      ELLE_TRACE_SCOPE("%s: connect to peer", *this);
      std::vector<std::unique_ptr<Round>> rounds;
      rounds.emplace_back(new AddressRound("local", this->peer_endpoints()));
      rounds.emplace_back(new FallbackRound("fallback",
                                            this->_owner.state().meta(),
                                            this->_owner.data()->id));
      return elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        reactor::Barrier found;
        std::unique_ptr<station::Host> host;
        scope.run_background(
          "wait_accepted",
          [&] ()
          {
            reactor::wait(this->_owner.station().host_available());
            std::unique_ptr<station::Host> res =
              this->_owner.station().accept();
            ELLE_ASSERT_NEQ(res, nullptr);
            ELLE_ASSERT_EQ(host, nullptr);
            host = std::move(res);
            found.open();
            ELLE_TRACE("%s: peer connection accepted", *this);
          });
        scope.run_background(
          "rounds",
          [&]
          {
            for (auto& round: rounds)
            { // try rounds in order: (currently local, apertus)
              std::unique_ptr<station::Host> res;
              res = round->connect(this->_owner.station());
              if (res)
              {
                ELLE_ASSERT_EQ(host, nullptr);
                host = std::move(res);
                found.open();
                if (this->_owner.state().metrics_reporter())
                  this->_owner.state().metrics_reporter()->transaction_connected(
                  this->_owner.transaction_id(),
                  round->name()
                );
                ELLE_TRACE("%s: connected to peer with %s",
                           *this, *round);
                break;
              }
              else
                ELLE_DEBUG("%s: connection round %s failed", *this, *round);
            }
          });
        reactor::wait(found);
        ELLE_ASSERT(host != nullptr);
        return std::move(host);
      };
      throw Exception(gap_api_error, "unable to connect to peer");
    }

    void
    PeerTransferMachine::_connection()
    {
      this->_host.reset();
      this->_host = this->_connect();
      ELLE_TRACE_SCOPE("%s: open peer to peer RPCs", *this);
      this->_serializer.reset(
        new infinit::protocol::Serializer(this->_host->socket()));
      this->_channels.reset(
        new infinit::protocol::ChanneledStream(*this->_serializer));
      this->_rpcs = this->_owner.rpcs(*this->_channels);
    }

    void
    PeerTransferMachine::_wait_for_peer()
    {
    }

    void
    PeerTransferMachine::_transfer()
    {
      elle::SafeFinally clear_frete{
        [this]
        {
          this->_rpcs.reset();
          this->_channels.reset();
          this->_serializer.reset();
          this->_host.reset();
        }};
      this->_owner._transfer_operation(*this->_rpcs);
      ELLE_TRACE_SCOPE("%s: end of transfer operation", *this);
    }

    void
    PeerTransferMachine::_stopped()
    {
    }

    void
    PeerTransferMachine::_cloud_buffer()
    {
      this->_owner._cloud_operation();
    }

    void
    PeerTransferMachine::_cloud_synchronize()
    {
      this->_owner._cloud_synchronize();
    }

    void
    PeerTransferMachine::_initialize()
    {
      // Clear eventually left over station host.
      // FIXME: now that _connection starts with a _host.reset, I don't think
      // this is needed anymore.
      this->_host.reset();
    }

    /*----------.
    | Printable |
    `----------*/

    void
    PeerTransferMachine::print(std::ostream& stream) const
    {
      stream << "PeerTransferMachine(" << this->_owner.id();
      if (this->_owner.data())
        stream << ", " << this->_owner.data()->id;
      stream << ")";
    }
  }
}
