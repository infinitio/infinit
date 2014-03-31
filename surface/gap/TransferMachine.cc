#include <boost/filesystem.hpp>

#include <elle/log.hh>
#include <elle/network/Interface.hh>
#include <elle/os/environ.hh>

#include <reactor/network/exception.hh>

#include <protocol/exceptions.hh>

#include <station/src/station/Station.hh>
#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/Rounds.hh>
#include <surface/gap/SendMachine.hh>
#include <surface/gap/State.hh>
#include <surface/gap/TransactionMachine.hh>
#include <surface/gap/TransferMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.TransferMachine");

namespace surface
{
  namespace gap
  {
    /*-------------.
    | Construction |
    `-------------*/

    TransferMachine::TransferMachine(TransactionMachine& owner):
      _owner(owner),
      _fsm(),
      _peer_online("peer online"),
      _peer_offline("peer offline"),
      _peer_reachable("peer reachable"),
      _peer_unreachable("peer unreachable"),
      _peer_connected("peer connected")
    {
      // Online / Offline barrier can't be initialized here, because
      // TransactionMachine is abstract.
      // I choosed to initialize the values on run() method.

      // By default, use the cached status from gap.
      this->_peer_unreachable.open();

      /*-------.
      | States |
      `-------*/
      auto& publish_interfaces_state =
        this->_fsm.state_make(
          "publish interfaces",
          std::bind(&TransferMachine::_publish_interfaces, this));
      auto& connection_state =
        this->_fsm.state_make(
          "connection",
          std::bind(&TransferMachine::_connection, this));
      auto& wait_for_peer_state =
        this->_fsm.state_make(
          "wait for peer",
          std::bind(&TransferMachine::_wait_for_peer, this));
      auto& cloud_buffer_state =
        this->_fsm.state_make(
          "cloud buffer",
          std::bind(&TransferMachine::_cloud_buffer, this));
      auto& transfer_state =
        this->_fsm.state_make(
          "transfer",
          std::bind(&TransferMachine::_transfer, this));
      auto& stopped_state =
        this->_fsm.state_make(
          "stopped",
          std::bind(&TransferMachine::_stopped, this));

      /*------------.
      | Transitions |
      `------------*/

      // Publish and wait for connection.
      this->_fsm.transition_add(
        publish_interfaces_state,
        connection_state,
        reactor::Waitables{&this->_peer_reachable});
      this->_fsm.transition_add(
        publish_interfaces_state,
        wait_for_peer_state,
        reactor::Waitables{&this->_peer_unreachable});
      this->_fsm.transition_add(
        connection_state,
        wait_for_peer_state,
        reactor::Waitables{&this->_peer_unreachable},
        true)
        .action([&]
                {
                  ELLE_TRACE("%s: peer went offline while connecting", *this);
                });
      this->_fsm.transition_add(
        wait_for_peer_state,
        connection_state,
        reactor::Waitables{&this->_peer_reachable},
        true)
        .action([&]
                {
                  ELLE_TRACE("%s: peer went online", *this);
                });

      // Cloud buffer.
      this->_fsm.transition_add(
        wait_for_peer_state,
        cloud_buffer_state,
        reactor::Waitables{&this->_peer_offline},
        true)
        .action([&]
                {
                  ELLE_TRACE("%s: start cloud buffering", *this);
                });
      this->_fsm.transition_add(
        cloud_buffer_state,
        wait_for_peer_state,
        reactor::Waitables{&this->_peer_online},
        true)
        .action([&]
                {
                  ELLE_TRACE("%s: stop cloud buffering", *this);
                });

      // Finished.
      this->_fsm.transition_add(
        transfer_state,
        stopped_state,
        reactor::Waitables{&owner.finished()},
        true)
        .action([this]
                {
                  ELLE_LOG("%s: transfer finished: owner.finished()", *this);
                });
      this->_fsm.transition_add(
        transfer_state,
        stopped_state,
        [this]() { return this->finished(); }
        )
        .action([this]
                {
                  ELLE_LOG("%s: transfer finished: this->finished", *this);
                });
      this->_fsm.transition_add(
        wait_for_peer_state,
        stopped_state,
        [this]() { return this->finished(); }
        )
        .action([this]
                {
                  ELLE_LOG("%s: transfer finished in the cloud", *this);
                });

      // Start and stop transfering.
      this->_fsm.transition_add(
        connection_state,
        transfer_state,
        reactor::Waitables{&this->_peer_connected});
      this->_fsm.transition_add_catch_specific<
        reactor::network::Exception>(
        transfer_state,
        connection_state)
        .action([this]
                {
                  ELLE_TRACE("%s: peer disconnected from the frete", *this);
                });
      this->_fsm.transition_add_catch_specific<
        infinit::protocol::Error>(
        transfer_state,
        connection_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_WARN("%s: protocol error in frete: %s",
                      *this, elle::exception_string(e));
          });
      this->_fsm.transition_add_catch_specific<
        infinit::protocol::ChecksumError>(
        transfer_state,
        connection_state)
        .action([this]
                {
                  ELLE_TRACE("%s: checksum error in frete", *this);
                });
      this->_fsm.transition_add(
        transfer_state,
        connection_state)
        .action([this]
                {
                  ELLE_TRACE("%s: peer disconnected from the frete", *this);
                });

      // Cancel.
      this->_fsm.transition_add(
        publish_interfaces_state,
        stopped_state,
        reactor::Waitables{&owner.canceled()}, true);
      this->_fsm.transition_add(
        connection_state,
        stopped_state,
        reactor::Waitables{&owner.canceled()}, true);
      this->_fsm.transition_add(
        wait_for_peer_state,
        stopped_state,
        reactor::Waitables{&owner.canceled()}, true);
      this->_fsm.transition_add(
        transfer_state,
        stopped_state,
        reactor::Waitables{&owner.canceled()}, true);

      // Failure.
      this->_fsm.transition_add_catch(
        publish_interfaces_state,
        stopped_state)
        .action_exception(
          [this, &owner] (std::exception_ptr exception)
          {
            ELLE_ERR("%s: interface publication failed: %s",
                     *this, elle::exception_string(exception));
            owner.failed().open();
          });
      this->_fsm.transition_add_catch(
        wait_for_peer_state,
        stopped_state)
        .action([this, &owner]
                {
                  ELLE_ERR("%s: peer wait failed", *this);
                  owner.failed().open();
                });
      this->_fsm.transition_add_catch(
        connection_state,
        stopped_state)
        .action_exception(
          [this, &owner] (std::exception_ptr e)
          {
            ELLE_ERR("%s: connection failed: %s",
                     *this, elle::exception_string(e));
            owner.failed().open();
          });
      this->_fsm.transition_add_catch(
        transfer_state,
        stopped_state)
        .action([this, &owner]
                {
                  ELLE_ERR("%s: transfer failed", *this);
                  owner.failed().open();
                });
    }

    /*--------.
    | Control |
    `--------*/

    void
    TransferMachine::run()
    {
      // XXX: Best place to do that? (See constructor).
      if (this->_owner.state().user(this->_owner.peer_id()).online())
      {
        this->_peer_offline.close();
        this->_peer_online.open();
      }
      else
      {
        this->_peer_online.close();
        this->_peer_offline.open();
      }

      this->_fsm.run();
    }

    /*-------.
    | Status |
    `-------*/

    float
    TransferMachine::progress() const
    {
      return this->_owner.progress();
    }

    bool
    TransferMachine::finished() const
    {
      if (auto owner = dynamic_cast<SendMachine*>(&this->_owner))
        return owner->frete().finished();
      else
        return false; // FIXME
    }

    /*-------.
    | States |
    `-------*/

    void
    TransferMachine::_publish_interfaces()
    {
      ELLE_TRACE_SCOPE("%s: publish interfaces", *this);
      this->_owner.current_state(TransactionMachine::State::PublishInterfaces);
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

    std::unique_ptr<reactor::network::Socket>
    TransferMachine::_connect()
    {
      ELLE_TRACE_SCOPE("%s: connect to peer", *this);
      auto const& transaction = *this->_owner.data();
      infinit::oracles::meta::EndpointNodeResponse endpoints;
      // Get endpoints. If the peer disconnected, we might go back to the
      // connection state before we're informed of this and try
      // reconnecting. Don't err and just keep retrying until either he comes
      // back online or we're notified he's offline.
      while (true)
      {
        try
        {
          ELLE_DEBUG_SCOPE("%s: get peer endpoints", *this);
          endpoints = this->_owner.state().meta().device_endpoints(
            this->_owner.data()->id,
            this->_owner.is_sender() ? transaction.sender_device_id :
            transaction.recipient_device_id,
            this->_owner.is_sender() ? transaction.recipient_device_id :
            transaction.sender_device_id);
          break;
        }
        catch (infinit::oracles::meta::Exception const& e)
        {
          ELLE_WARN("%s: unable to fetch endpoints: %s", *this, e);
          if (e.err != infinit::oracles::meta::Error::device_not_found)
            throw;
        }
        reactor::sleep(1_sec);
      }
      ELLE_DEBUG("%s: got endpoints", *this)
      {
        ELLE_DEBUG("locals: %s", endpoints.locals);
        ELLE_DEBUG("externals: %s", endpoints.externals);
      }
      std::vector<std::unique_ptr<Round>> rounds;
      rounds.emplace_back(new AddressRound("local",
                                           std::move(endpoints.locals)));
      rounds.emplace_back(new FallbackRound("fallback",
                                            this->_owner.state().meta(),
                                            this->_owner.data()->id));
      return elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        reactor::Barrier found;
        std::unique_ptr<reactor::network::Socket> host;
        scope.run_background(
          "wait_accepted",
          [&] ()
          {
            reactor::wait(this->_owner.station().host_available());
            host = this->_owner.station().accept()->release();
            found.open();
            ELLE_TRACE("%s: peer connection accepted", *this);
          });
        scope.run_background(
          "rounds",
          [&]
          {
            for (auto& round: rounds)
            {
              host = round->connect(this->_owner.station());
              if (host)
              {
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
    TransferMachine::_connection()
    {
      ELLE_TRACE_SCOPE("%s: connect to peer", *this);
      this->_owner.current_state(TransactionMachine::State::Connect);
      this->_host = this->_connect();
      this->_serializer.reset(
        new infinit::protocol::Serializer(*this->_host));
      this->_channels.reset(
        new infinit::protocol::ChanneledStream(*this->_serializer));
      this->_rpcs = this->_owner.rpcs(*this->_channels);
      this->_peer_connected.signal();
    }

    static std::streamsize const chunk_size = 1 << 18;

    void
    TransferMachine::_wait_for_peer()
    {
      this->_owner.current_state(TransactionMachine::State::PeerDisconnected);
      ELLE_TRACE_SCOPE("%s: wait for peer to connect", *this);
    }

    void
    TransferMachine::_transfer()
    {
      ELLE_TRACE_SCOPE("%s: transfer", *this);
      this->_owner.current_state(TransactionMachine::State::Transfer);
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
    TransferMachine::_stopped()
    {
      ELLE_TRACE_SCOPE("%s: stopped", *this);
    }

    void
    TransferMachine::_cloud_buffer()
    {
      this->_owner.current_state(TransactionMachine::State::Transfer);
      this->_owner._cloud_operation();
    }

    /*----------.
    | Printable |
    `----------*/

    void
    TransferMachine::print(std::ostream& stream) const
    {
      stream << "TransferMachine(" << this->_owner.id() << ")";
    }
  }
}
