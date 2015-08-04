#include <elle/log.hh>

#include <infinit/oracles/meta/Client.hh>
#include <surface/gap/Error.hh>
#include <surface/gap/Exception.hh>
#include <surface/gap/PeerMachine.hh>
#include <surface/gap/PeerTransferMachine.hh>
#include <surface/gap/State.hh>

ELLE_LOG_COMPONENT("surface.gap.PeerMachine");

namespace surface
{
  namespace gap
  {
    PeerMachine::PeerMachine(Transaction& transaction,
                             uint32_t id,
                             std::shared_ptr<Data> data,
                             papier::Authority const& authority)
      : Super(transaction, id, data)
      , _data(data)
      , _authority(authority)
      , _transfer_machine(new PeerTransferMachine(*this))
    {
      this->_machine.transition_add(
        this->_transfer_state,
        this->_transfer_state,
        reactor::Waitables{&this->reset_transfer_signal()},
        true);
    }

    void
    PeerMachine::_transfer()
    {
      ELLE_TRACE_SCOPE("%s: start transfer machine", *this);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      try
      {
        this->_transfer_machine->run();
        ELLE_TRACE("%s: transfer machine finished properly", *this);
      }
      catch (infinit::state::TransactionFinalized const&)
      {
        // Nothing to do, some kind of transition should push us to another
        // final state.
        ELLE_TRACE(
          "%s: transfer machine was stopped because transaction was finalized",
          *this);
      }
      catch (reactor::Terminate const&)
      {
        ELLE_TRACE("%s: terminated", *this);
        throw;
      }
      catch (elle::Exception const& e)
      {
        // This should not happen, exceptions should be intercepted lower in
        // the stack, and fail or cancel the transaction
        ELLE_ERR("%s: something went wrong while transfering: %s",
                 *this, e);
        throw;
      }
      catch (std::exception const&)
      {
        ELLE_ERR("%s: something went wrong while transfering: %s",
                 *this, elle::exception_string());
        throw;
      }
      if (this->failed().opened())
        throw Exception(gap_error, "an error occured");
    }

    void
    PeerMachine::peer_available(
      std::vector<std::pair<std::string, int>> const& local_endpoints,
      std::vector<std::pair<std::string, int>> const& public_endpoints
      )
    {
      ELLE_TRACE_SCOPE("%s: peer is available for peer to peer connection",
                       *this);
      this->_transfer_machine->peer_available(local_endpoints, public_endpoints);
    }

    void
    PeerMachine::peer_unavailable()
    {
      ELLE_TRACE_SCOPE("%s: peer is unavailable for peer to peer connection",
                       *this);
      this->_transfer_machine->peer_unavailable();
    }

    void
    PeerMachine::_peer_connection_changed(bool user_status)
    {
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      if (user_status)
        ELLE_TRACE("%s: peer is now online", *this)
        {
          this->_transfer_machine->peer_offline().close();
          this->_transfer_machine->peer_online().open();
        }
      else
        ELLE_TRACE("%s: peer is now offline", *this)
        {
          this->_transfer_machine->peer_online().close();
          this->_transfer_machine->peer_offline().open();
        }
    }

    float
    PeerMachine::progress() const
    {
      return this->_transfer_machine->progress();
    }

    std::unique_ptr<infinit::oracles::meta::CloudCredentials>
    PeerMachine::_cloud_credentials(bool first_time)
    {
      auto& meta = this->state().meta();
      int delay = 1;
      std::unique_ptr<infinit::oracles::meta::CloudCredentials> credentials;
      while (true)
      {
        try
        {
          credentials =
            meta.get_cloud_buffer_token(this->transaction_id(), !first_time);
          break;
        }
        catch (elle::http::Exception const&)
        {
          ELLE_LOG("%s: get_cloud_buffer_token failed with %s, "
                   "retrying in %s...", *this, elle::exception_string(), delay);
          reactor::sleep(boost::posix_time::seconds(delay));
          delay = std::min(delay * 2, 60 * 10);
        }
      }
      return std::move(credentials);
    }

    void
    PeerMachine::_update_meta_status(infinit::oracles::Transaction::Status s)
    {
      this->state().meta().update_transaction(this->transaction_id(),
                                              s,
                                              this->state().device().id,
                                              this->state().device().name);
    }

    void
    PeerMachine::_pause(bool pause_action)
    {
      this->state().meta().update_transaction(this->transaction_id(),
                                              boost::none, //status
                                              elle::UUID(), // device_id
                                              "", // device_name
                                              pause_action);
    }
  }
}
