#include "ReceiveMachine.hh"

#include <surface/gap/_detail/TransferOperations.hh>

#include <hole/Passport.hh>
#include <lune/Identity.hh>

#include <reactor/thread.hh>
#include <elle/os/getenv.hh>
#include <elle/network/Interface.hh>


ELLE_LOG_COMPONENT("surface.gap.ReceiveMachine");

namespace surface
{
  namespace gap
  {
    ReceiveMachine::ReceiveMachine(surface::gap::State const& state):
      TransferMachine(state),
      _wait_for_decision_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_wait_for_decision, this))),
      _accept_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_accept, this))),
      _reject_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_reject, this))),
      _publish_interfaces_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_publish_interfaces, this))),
      _connection_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_connection, this))),
      _transfer_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_transfer, this))),
      _clean_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_clean, this))),
      _fail_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_fail, this)))
    {
      this->_machine.transition_add(_wait_for_decision_state,
                                    _accept_state,
                                    reactor::Waitables{&_accepted});

      this->_machine.transition_add(_wait_for_decision_state,
                                    _reject_state,
                                    reactor::Waitables{&_rejected});

      this->_machine.transition_add(_accept_state,
                                    _publish_interfaces_state,
                                    reactor::Waitables{&_ready});

      this->_machine.transition_add(_publish_interfaces_state,
                                    _connection_state,
                                    reactor::Waitables{&_peer_online});
      this->_machine.transition_add(_connection_state,
                                    _publish_interfaces_state,
                                    reactor::Waitables{&_peer_offline});

      this->_machine.transition_add(_connection_state,
                                    _connection_state,
                                    reactor::Waitables{&_peer_online});

      this->_machine.transition_add(_connection_state,
                                    _transfer_state,
                                    reactor::Waitables{&_peer_connected});
      this->_machine.transition_add(_transfer_state,
                                    _connection_state,
                                    reactor::Waitables{&_peer_disconnected});

      this->_machine.transition_add(_transfer_state,
                                    _clean_state);
      // Exception handling.
      // this->_m.transition_add_catch(_request_network_state, _fail);

      this->run();
    }

    ReceiveMachine::~ReceiveMachine()
    {
      this->_stop();
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   std::string const& transaction_id):
      ReceiveMachine(state)
    {
      ELLE_TRACE_SCOPE("%s: constructing machine for transaction %s",
                       *this, transaction_id);
      this->transaction_id(transaction_id);
    }

    void
    ReceiveMachine::on_transaction_update(plasma::Transaction const& transaction)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction %s",
                       *this, transaction);

      ELLE_ASSERT_EQ(this->transaction_id(), transaction.id);
      switch (transaction.status)
      {
        // case plasma::TransactionStatus::accepted:
        //   this->_accepted.signal();
        //   break;
        case plasma::TransactionStatus::canceled:
          this->_canceled.signal();
          break;
        case plasma::TransactionStatus::failed:
          this->_failed.signal();
          break;
        case plasma::TransactionStatus::finished:
          this->_finished.signal();
          break;
        case plasma::TransactionStatus::ready:
          this->_ready.signal();
          break;
        case plasma::TransactionStatus::created:
        case plasma::TransactionStatus::initialized:
        case plasma::TransactionStatus::rejected:
        case plasma::TransactionStatus::_count:
          break;
      }
    }

    void
    ReceiveMachine::on_user_update(plasma::meta::User const& user)
    {
    }

    void
    ReceiveMachine::on_network_update(plasma::meta::NetworkResponse const& network)
    {
    }

    void
    ReceiveMachine::accept()
    {
      ELLE_TRACE_SCOPE("%s: accept transaction %s", *this, this->transaction_id());
      this->_accepted.signal();
    }

    void
    ReceiveMachine::rejected()
    {
      ELLE_TRACE_SCOPE("%s: reject transaction %s", *this, this->transaction_id());
      this->_rejected.signal();
    }

    void
    ReceiveMachine::_wait_for_decision()
    {
      this->network_id(this->state().meta().transaction(this->transaction_id()).network_id);
    }

    void
    ReceiveMachine::_accept()
    {
      this->state().meta().update_transaction(this->transaction_id(),
                                              plasma::TransactionStatus::accepted,
                                              this->state().device_id(),
                                              "bite");
    }

    void
    ReceiveMachine::_reject()
    {
      this->state().meta().update_transaction(this->transaction_id(),
                                              plasma::TransactionStatus::rejected);
    }


    // XXX: Same for sender and recipient.
    void
    ReceiveMachine::_publish_interfaces()
    {
      typedef std::vector<std::pair<std::string, uint16_t>> AddressContainer;
      AddressContainer addresses;

      // In order to test the fallback, we can fake our local addresses.
      // It should also work for nated network.
      if (elle::os::getenv("INFINIT_LOCAL_ADDRESS", "").length() > 0)
      {
        addresses.emplace_back(elle::os::getenv("INFINIT_LOCAL_ADDRESS"),
                               this->hole().port());
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
            addresses.emplace_back(ipv4, this->hole().port());
          }
      }
      ELLE_DEBUG("addresses: %s", addresses);

      AddressContainer public_addresses;

      this->state().meta().network_connect_device(
        this->network_id(), this->state().passport().id(), addresses, public_addresses);
    }

    void
    ReceiveMachine::_connection()
    {

    }

    void
    ReceiveMachine::_transfer()
    {

    }

    void
    ReceiveMachine::_clean()
    {
    }

    void
    ReceiveMachine::_fail()
    {
    }

  }
}
