#include <boost/filesystem.hpp>

#include <elle/container/map.hh>
#include <elle/log.hh>
#include <elle/network/Interface.hh>
#include <elle/os/environ.hh>

#include <reactor/network/exception.hh>

#include <protocol/exceptions.hh>

#include <station/Station.hh>

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
    PeerTransferMachine::PeerTransferMachine(PeerMachine& owner)
      : Transferer(owner)
      , _owner(owner)
      , _station(papier::authority(),
                 this->_owner.state().passport(),
                 elle::sprintf("Station(id=%s)", this->_owner.id()))
      , _upnp(reactor::network::UPNP::make())
      , _upnp_init_thread(elle::sprintf("%s UPNP init thread", *this),
                      [this] { this->_upnp_init(); })
    {
      ELLE_TRACE("%s: created", *this);
    }

    PeerTransferMachine::~PeerTransferMachine() noexcept(true)
    {
      ELLE_TRACE("%s: destroyed", *this);
      this->_upnp_init_thread.terminate_now();
    }

    void
    PeerTransferMachine::_upnp_init()
    {
      if (this->_owner.state().configuration().disable_upnp
          || !elle::os::getenv("INFINIT_DISABLE_UPNP", "").empty())
      {
        ELLE_TRACE("%s: UPNP support disabled by configuration", *this);
        return;
      }
      // Try to acquire a port mapping for the station port in the background
      ELLE_TRACE("%s: initialize UPNP", *this)
        try
        {
          this->_upnp->initialize();
          ELLE_TRACE("%s: UPNP initialized, available=%s",
                     *this, this->_upnp->available());
        }
        // FIXME: use elle::Error
        catch (reactor::Terminate const&)
        {
          throw;
        }
        catch (std::exception const& e)
        {
          ELLE_LOG("%s: UPNP initialization failed: %s", *this, e.what());
          return;
        }
      ELLE_TRACE("%s: acquire UPNP mapping", *this)
        try
        {
          this->_upnp_mapping =
            this->_upnp->setup_redirect(reactor::network::Protocol::tcp,
                                        this->_station.port());
           ELLE_TRACE("%s: acquired UPNP mapping: %s",
                      *this, this->_upnp_mapping);
        }
        // FIXME: use elle::Error
        catch (reactor::Terminate const&)
        {
          throw;
        }
        catch (std::exception const& e)
        {
          ELLE_LOG("%s: UPNP mapping failed: %s", *this, e.what());
          return;
        }
    }

    void
    PeerTransferMachine::_publish_interfaces()
    {
      typedef std::vector<std::pair<std::string, uint16_t>> AddressContainer;
      AddressContainer addresses;
      // In order to test the fallback, we can fake our local addresses.
      // It should also work for nated network.
      if (elle::os::getenv("INFINIT_LOCAL_ADDRESS", "").length() > 0)
      {
        addresses.emplace_back(elle::os::getenv("INFINIT_LOCAL_ADDRESS"),
                               this->_station.port());
      }
      else
      {
        try
        {
          auto interfaces = elle::network::Interface::get_map(
            elle::network::Interface::Filter::only_up |
            elle::network::Interface::Filter::no_loopback |
            elle::network::Interface::Filter::no_autoip
            );
          for (auto const& pair: interfaces)
            if (pair.second.ipv4_address.size() > 0)
            {
              auto const& ipv4 = pair.second.ipv4_address;
              addresses.emplace_back(ipv4, this->_station.port());
            }
        }
        catch (elle::Exception const& e)
        {
          ELLE_ERR("%s: unable to get user interfaces: %s", *this, e);
          // XXX: Add a metric.
        }
      }
      AddressContainer public_addresses;
      if (elle::os::getenv("INFINIT_UPNP_ADDRESS", "").length() > 0)
      {
        public_addresses.emplace_back(elle::os::getenv("INFINIT_UPNP_ADDRESS"),
                               this->_station.port());
      }
      else if (this->_upnp_mapping)
      {
        public_addresses.emplace_back(this->_upnp_mapping.external_host,
                                      boost::lexical_cast<unsigned short>(
                                        this->_upnp_mapping.external_port));
      }
      ELLE_DEBUG("addresses: local=%s, public=%s", addresses, public_addresses);
      this->_owner.state().meta().transaction_endpoints_put(
        this->_owner.data()->id,
        this->_owner.state().device().id,
        addresses,
        public_addresses);
    }

    std::unique_ptr<station::Host>
    PeerTransferMachine::_connect()
    {
      ++_attempt;
      ELLE_TRACE_SCOPE("%s: connect to peer", *this);
      std::vector<std::unique_ptr<Round>> rounds;
      rounds.emplace_back(new AddressRound("local", this->peer_local_endpoints()));
      auto all_endpoints = this->peer_local_endpoints();
      for (auto const& ep: this->peer_public_endpoints())
        all_endpoints.push_back(ep);
      rounds.emplace_back(new AddressRound("upnp", all_endpoints));
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
            ELLE_TRACE_SCOPE("%s: wait for peer connection", *this);
            std::unique_ptr<station::Host> res =
              this->_station.accept();
            ELLE_TRACE("%s: peer connection accepted", *this);
            ELLE_ASSERT_NEQ(res, nullptr);
            ELLE_ASSERT_EQ(host, nullptr);
            host = std::move(res);
            found.open();
          });
        scope.run_background(
          "rounds",
          [&]
          {
            for (auto& round: rounds)
            { // try rounds in order: (currently local, upnp, apertus)
              ELLE_DEBUG("%s: starting connection round %s", *this, *round);
              std::unique_ptr<station::Host> res;
              res = round->connect(this->_station);
              if (res)
              {
                ELLE_ASSERT_EQ(host, nullptr);
                host = std::move(res);
                found.open();
                bool skip_report = (_attempt > 10 && _attempt % (unsigned)pow(10, (unsigned)log10(_attempt)));
                if (!skip_report && this->_owner.state().metrics_reporter())
                {
                  this->_owner.state().metrics_reporter()->transaction_connected(
                    this->_owner.transaction_id(),
                    round->name(),
                    _attempt
                    );
                }
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
      if (!this->_owner.data()->id.empty())
        stream << ", " << this->_owner.data()->id;
      stream << ")";
    }
  }
}
