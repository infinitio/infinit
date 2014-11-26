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
                             std::shared_ptr<Data> data)
      : Super(transaction, id, data)
      , _data(data)
      , _transfer_machine(new PeerTransferMachine(*this))
      , _transfer_core_state(
        this->_machine.state_make(
          "transfer core", std::bind(&PeerMachine::_transfer_core, this)))
    {
      this->_machine.transition_add(
        this->_transfer_core_state,
        this->_pause_state,
        reactor::Waitables{&this->paused()},
        true);
      this->_machine.transition_add(
        this->_pause_state,
        this->_transfer_core_state,
        reactor::Waitables{&this->resumed()});
      this->_machine.transition_add(
        this->_transfer_core_state,
        this->_finish_state,
        reactor::Waitables{&this->finished()},
        true);
      this->_machine.transition_add(
        this->_transfer_core_state,
        this->_cancel_state,
        reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add_catch(
        this->_transfer_core_state,
        this->_fail_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_WARN("%s: error while transfering: %s",
                      *this, elle::exception_string(e));
            this->transaction().failure_reason(elle::exception_string(e));
          });
      this->_machine.transition_add(
        this->_transfer_core_state,
        this->_fail_state,
        reactor::Waitables{&this->failed()}, true);
      this->_machine.transition_add(
        this->_transfer_core_state,
        this->_transfer_core_state,
        reactor::Waitables{&this->reset_transfer_signal()},
        true);
    }

    void
    PeerMachine::_transfer_core()
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

    aws::Credentials
    PeerMachine::_aws_credentials(bool first_time)
    {
      auto& meta = this->state().meta();
      int delay = 1;
      aws::Credentials credentials;
      while (true)
      {
        try
        {
          credentials =
            meta.get_cloud_buffer_token(this->transaction_id(), !first_time);
          break;
        }
        catch(infinit::oracles::meta::Exception const&)
        {
          ELLE_LOG("%s: get_cloud_buffer_token failed with %s, retrying...",
                   *this, elle::exception_string());
          // if meta looses connectivity to provider let's not flood it
          reactor::sleep(boost::posix_time::seconds(delay));
          delay = std::min(delay * 2, 60 * 10);
        }
      }
      return credentials;
    }

    void
    PeerMachine::_finalize(infinit::oracles::Transaction::Status status)
    {
      ELLE_TRACE_SCOPE("%s: finalize machine: %s", *this, status);
      if (!this->data()->id.empty())
      {
        try
        {
          this->data()->status = status;
          // The status should only be set to finished by the recipient unless
          // the recipient is a ghost.
          auto peer = this->state().user(this->data()->recipient_id);
          auto self_id = this->state().me().id;
          auto self_device_id = this->state().device().id;
          if (status == infinit::oracles::Transaction::Status::ghost_uploaded)
          {
            ELLE_ASSERT(peer.ghost() || transaction().data()->is_ghost);
            ELLE_TRACE_SCOPE("%s: notifying meta of finished state", *this);
            this->state().meta().update_transaction(
              this->transaction_id(),
              status);
          }
          else if (status == infinit::oracles::Transaction::Status::finished &&
                   self_id == this->data()->recipient_id &&
                   self_device_id == this->data()->recipient_device_id)
          {
            this->state().meta().update_transaction(
              this->transaction_id(), status);
          }
          else if (status != infinit::oracles::Transaction::Status::finished)
          {
            this->state().meta().update_transaction(
              this->transaction_id(), status);
          }
        }
        catch (infinit::oracles::meta::Exception const& e)
        {
          using infinit::oracles::meta::Error;
          if (e.err == Error::transaction_already_finalized)
            ELLE_TRACE("%s: transaction already finalized", *this);
          else if (e.err == Error::transaction_already_has_this_status)
            ELLE_TRACE("%s: transaction already in this state", *this);
          else
            ELLE_ERR("%s: unable to finalize the transaction %s: %s",
                     *this, this->transaction_id(), elle::exception_string());
        }
        catch (std::exception const&)
        {
          ELLE_ERR("%s: unable to finalize the transaction %s: %s",
                   *this, this->transaction_id(), elle::exception_string());
        }
      }
      else
      {
        ELLE_ERR("%s: can't finalize transaction: id is still empty", *this);
      }
      this->cleanup();
    }
  }
}
