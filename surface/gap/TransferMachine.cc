#include <boost/filesystem.hpp>

#include <elle/container/vector.hh>
#include <elle/container/map.hh>
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

    BaseTransferer::BaseTransferer(TransactionMachine& owner)
    : _peer_online("peer online")
    , _peer_offline("peer offline")
    , _owner(owner)
    {
    }

    Transferer::Transferer(TransactionMachine& owner):
      BaseTransferer(owner),
      _fsm(elle::sprintf("transfer (%s) fsm", owner.id())),
      _peer_reachable("peer reachable"),
      _peer_unreachable("peer unreachable")
    {
      // Online / Offline barrier can't be initialized here, because
      // TransactionMachine is abstract.
      // I choosed to initialize the values on run() method.

      // By default, use the cached status from gap.
      this->_peer_unreachable.open();

      /*-------.
      | States |
      `-------*/
      // fetch all you can from cloud
      auto& cloud_synchronize_state =
        this->_fsm.state_make(
          "cloud synchronize",
          std::bind(&Transferer::_cloud_synchronize_wrapper, this));
      auto& publish_interfaces_state =
        this->_fsm.state_make(
          "publish interfaces",
          std::bind(&Transferer::_publish_interfaces_wrapper, this));
      auto& connection_state =
        this->_fsm.state_make(
          "connection",
          std::bind(&Transferer::_connection_wrapper, this));
      auto& wait_for_peer_state =
        this->_fsm.state_make(
          "wait for peer",
          std::bind(&Transferer::_wait_for_peer_wrapper, this));
      auto& transfer_state =
        this->_fsm.state_make(
          "transfer",
          std::bind(&Transferer::_transfer_wrapper, this));
      auto& cloud_buffer_state =
        this->_fsm.state_make(
          "cloud buffer",
          std::bind(&Transferer::_cloud_buffer_wrapper, this));
      auto& stopped_state =
        this->_fsm.state_make(
          "stopped",
          std::bind(&Transferer::_stopped_wrapper, this));

      /*------------.
      | Transitions |
      `------------*/

      // first thing is cloud sync, and only when done do we publis ifaces
      this->_fsm.transition_add(
        cloud_synchronize_state,
        stopped_state,
        reactor::Waitables{&owner.finished()}
        );
      this->_fsm.transition_add(
        cloud_synchronize_state,
        publish_interfaces_state);
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
        reactor::Waitables{&this->peer_offline()},
        true)
        .action([&]
                {
                  ELLE_TRACE("%s: start cloud buffering", *this);
                });
      this->_fsm.transition_add(
        cloud_buffer_state,
        wait_for_peer_state,
        reactor::Waitables{&this->peer_online()},
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
        wait_for_peer_state,
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
        cloud_buffer_state,
        stopped_state,
        [this]() { return this->finished(); }
        )
        .action([this]
                {
                  ELLE_LOG("%s: transfer finished in the cloud", *this);
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
        transfer_state);
      // In case network is lost abruptly, trophonius might get notified
      // before us: we will recieve a peer-offline event before
      // a disconnection on our p2p link.
      this->_fsm.transition_add(
        transfer_state,
        connection_state,
        reactor::Waitables{&this->peer_offline()},
        true);
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
      this->_fsm.transition_add_catch(
        transfer_state,
        stopped_state)
        .action_exception([this, &owner](std::exception_ptr exception)
                {
                  ELLE_TRACE("%s: failing transfer because of exception: %s",
                             *this, elle::exception_string(exception));
                  owner.failed().open();
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
      // On network error while connecting, retry. For instance if the
      // connection get lost while negociating RPCs, or if the peer fumbles and
      // closes connection.
      this->_fsm.transition_add_catch_specific<reactor::network::Exception>(
        connection_state,
        connection_state)
        .action_exception(
          [this, &owner] (std::exception_ptr e)
          {
            ELLE_TRACE("%s: network error on connection: %s",
                     *this, elle::exception_string(e));
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

      this->_fsm.state_changed().connect(
        [this] (reactor::fsm::State& state)
        {
          ELLE_LOG_COMPONENT("surface.gap.Transferer.State");
          ELLE_TRACE("%s: entering %s", *this, state);
        });

      this->_fsm.transition_triggered().connect(
        [this] (reactor::fsm::Transition& transition)
        {
          ELLE_LOG_COMPONENT("surface.gap.Transferer.Transition");
          ELLE_TRACE("%s: %s triggered", *this, transition);
        });

    }

    /*--------.
    | Control |
    `--------*/

    void
    Transferer::run()
    {
      ELLE_TRACE_SCOPE("%s: run fsm %s", *this, this->_fsm);
      // XXX: Best place to do that? (See constructor).
      if (this->_owner.state().user(this->_owner.peer_id()).online())
      {
        this->peer_offline().close();
        this->peer_online().open();
      }
      else
      {
        this->peer_online().close();
        this->peer_offline().open();
      }
      this->_initialize();
      this->_fsm.run();
    }

    /*---------.
    | Triggers |
    `---------*/

    void
    Transferer::peer_available(
      std::vector<std::pair<std::string, int>> const& endpoints)
    {
      ELLE_TRACE_SCOPE("%s: peer is available on %s", *this, endpoints);
      this->_peer_endpoints = endpoints;
      this->_peer_unreachable.close();
      this->_peer_reachable.open();
    }

    void
    Transferer::peer_unavailable()
    {
      ELLE_TRACE_SCOPE("%s: peer is unavailable", *this);
      this->_peer_endpoints.clear();
      this->_peer_reachable.close();
      this->_peer_unreachable.open();
    }

    /*-------.
    | Status |
    `-------*/

    float
    BaseTransferer::progress() const
    {
      return this->_owner.progress();
    }

    bool
    BaseTransferer::finished() const
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
    Transferer::_publish_interfaces_wrapper()
    {
      ELLE_TRACE_SCOPE("%s: publish interfaces", *this);
      // If the progress is full but the transaction is not finished
      // yet, it must be cloud buffered.
      if (this->progress() == 1)
        this->_owner.gap_state(gap_transaction_cloud_buffered);
      else
        this->_owner.gap_state(gap_transaction_connecting);
      this->_publish_interfaces();
    }

    void
    Transferer::_connection_wrapper()
    {
      ELLE_TRACE_SCOPE("%s: connect to peer", *this);
      this->_owner.gap_state(gap_transaction_connecting);
      this->_connection();
    }

    void
    Transferer::_wait_for_peer_wrapper()
    {
      ELLE_TRACE_SCOPE("%s: wait for peer to connect", *this);
      if (this->progress() == 1)
        this->_owner.gap_state(gap_transaction_cloud_buffered);
      else
        this->_owner.gap_state(gap_transaction_connecting);
      this->_wait_for_peer();
    }

    void
    Transferer::_cloud_buffer_wrapper()
    {
      ELLE_TRACE_SCOPE("%s: cloud buffer", *this);
      this->_cloud_buffer();
    }

    void
    Transferer::_cloud_synchronize_wrapper()
    {
      ELLE_TRACE_SCOPE("%s: cloud synchronize", *this);
      if (this->progress() == 1)
        this->_owner.gap_state(gap_transaction_cloud_buffered);
      else
        this->_owner.gap_state(gap_transaction_connecting);
      this->_cloud_synchronize();
    }

    void
    Transferer::_transfer_wrapper()
    {
      ELLE_TRACE_SCOPE("%s: transfer", *this);
      this->_owner.gap_state(gap_transaction_transferring);
      this->_transfer();
    }

    void
    Transferer::_stopped_wrapper()
    {
      ELLE_TRACE_SCOPE("%s: stopped", *this);
      this->_stopped();
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Transferer::print(std::ostream& stream) const
    {
      stream << "Transferer(" << this->_owner.id() << ")";
    }
  }
}
