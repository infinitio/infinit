#include <surface/gap/SendMachine.hh>
#include <surface/gap/Rounds.hh>
#include <surface/gap/_detail/TransferOperations.hh>

#include <papier/Descriptor.hh>

#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Subject.hh>

#include <reactor/thread.hh>
#include <reactor/exception.hh>

#include <elle/os/path.hh>
#include <elle/os/file.hh>

#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("surface.gap.SendMachine");

namespace surface
{
  namespace gap
  {
    using TransactionStatus = plasma::TransactionStatus;
    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::shared_ptr<TransferMachine::Data> data,
                             bool):
      TransferMachine(state, id, std::move(data)),
      _request_network_state(
        this->_machine.state_make(
          "request network", std::bind(&SendMachine::_request_network, this))),
      _create_transaction_state(
        this->_machine.state_make(
          "create transaction", std::bind(&SendMachine::_create_transaction, this))),
      _copy_files_state(
        this->_machine.state_make(
          "copy files", std::bind(&SendMachine::_copy_files, this))),
      _wait_for_accept_state(
        this->_machine.state_make(
          "wait for accept", std::bind(&SendMachine::_wait_for_accept, this))),
      _set_permissions_state(
        this->_machine.state_make(
          "set permissions", std::bind(&SendMachine::_set_permissions, this)))
    {
      this->_machine.transition_add(
        this->_request_network_state, this->_create_transaction_state);
      this->_machine.transition_add(
        this->_create_transaction_state, this->_copy_files_state);
      this->_machine.transition_add(
        this->_copy_files_state, this->_wait_for_accept_state);

      this->_machine.transition_add(
        this->_wait_for_accept_state,
        this->_set_permissions_state,
        reactor::Waitables{&this->_accepted},
        false,
        [&] () -> bool
        {
          return false;
          return this->state().transactions().at(this->id()).data()->status ==
            TransactionStatus::accepted;
        }
        );

      this->_machine.transition_add(
        this->_wait_for_accept_state,
        this->_reject_state,
        reactor::Waitables{&this->_rejected},
        false,
        [&] () -> bool
        {
          return false;
          return this->state().transactions().at(this->id()).data()->status ==
            TransactionStatus::rejected;
        }
        );

      this->_machine.transition_add(
        this->_set_permissions_state, this->_transfer_core_state);

      // Cancel.
      this->_machine.transition_add(this->_request_network_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(this->_create_transaction_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(this->_copy_files_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(this->_wait_for_accept_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(this->_set_permissions_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      // Exception.
      this->_machine.transition_add_catch(
        this->_request_network_state, this->_fail_state);
      this->_machine.transition_add_catch(
        this->_create_transaction_state, this->_fail_state);
      this->_machine.transition_add_catch(
        this->_copy_files_state, this->_fail_state);
      this->_machine.transition_add_catch(
        this->_wait_for_accept_state, this->_fail_state);
      this->_machine.transition_add_catch(
        this->_set_permissions_state, this->_fail_state);
    }

    SendMachine::~SendMachine()
    {
      this->_stop();
    }

    SendMachine::Snapshot
    SendMachine::_make_snapshot() const
    {
      return Snapshot{*this->data(), this->_current_state, this->_files, this->_message};
    }

    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::shared_ptr<TransferMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_WARN_SCOPE("%s: constructing machine for transaction data %s "
                      "(not found on local snapshots)",
                       *this, this->data());

      switch (this->data()->status)
      {
        case plasma::TransactionStatus::initialized:
          this->_run(this->_wait_for_accept_state);
          break;
        case plasma::TransactionStatus::accepted:
          this->_run(this->_set_permissions_state);
          break;
        case plasma::TransactionStatus::ready:
          this->_run(this->_transfer_core_state);
          break;
        case plasma::TransactionStatus::finished:
          this->_run(this->_finish_state);
          break;
        case plasma::TransactionStatus::canceled:
          this->_run(this->_cancel_state);
          break;
        case plasma::TransactionStatus::failed:
          this->_run(this->_fail_state);
          break;
        case plasma::TransactionStatus::rejected:
        case plasma::TransactionStatus::created:
          break;
        case plasma::TransactionStatus::started:
        case plasma::TransactionStatus::none:
        case plasma::TransactionStatus::_count:
          elle::unreachable();
      }
    }

    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::string const& recipient,
                             std::unordered_set<std::string>&& files,
                             std::string const& message,
                             std::shared_ptr<TransferMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: send %s to %s", *this, files, recipient);

      this->_message = message;

      if (files.empty())
        throw elle::Exception("no files to send");

      std::swap(this->_files, files);

      ELLE_ASSERT_NEQ(this->_files.size(), 0u);

      this->peer_id(recipient);
      this->_run(this->_request_network_state);
    }

    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::unordered_set<std::string> files,
                             TransferState current_state,
                             std::string const& message,
                             std::shared_ptr<TransferMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: construct from data %s, starting at %s",
                       *this, *this->data(), current_state);
      this->_files = std::move(files);
      this->_current_state = current_state;
      this->_message = message;
      switch (current_state)
      {
        case TransferState_NewTransaction:
          elle::unreachable();
        case TransferState_SenderCreateNetwork:
          this->_run(this->_request_network_state);
          break;
        case TransferState_SenderCreateTransaction:
          this->_run(this->_create_transaction_state);
          break;
        case TransferState_SenderCopyFiles:
          this->_run(this->_copy_files_state);
          break;
        case TransferState_SenderWaitForDecision:
          this->_run(this->_wait_for_accept_state);
          break;
        case TransferState_RecipientWaitForDecision:
        case TransferState_RecipientAccepted:
          elle::unreachable();
        case TransferState_GrantPermissions:
          this->_run(this->_set_permissions_state);
          break;
        case TransferState_PublishInterfaces:
        case TransferState_Connect:
        case TransferState_PeerDisconnected:
        case TransferState_PeerConnectionLost:
        case TransferState_Transfer:
          this->_run(this->_transfer_core_state);
          break;
        case TransferState_CleanLocal:
          this->_run(this->_local_clean_state);
          break;
        case TransferState_CleanRemote:
          this->_run(this->_remote_clean_state);
          break;
        case TransferState_Finished:
          this->_run(this->_finish_state);
          break;
        case TransferState_Rejected:
          this->_run(this->_reject_state);
          break;
        case TransferState_Canceled:
          this->_run(this->_cancel_state);
          break;
        case TransferState_Failed:
          this->_run(this->_fail_state);
          break;
        default:
          elle::unreachable();
      }
      this->current_state(current_state);
    }

    void
    SendMachine::transaction_status_update(plasma::TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      switch (status)
      {
        case plasma::TransactionStatus::accepted:
          ELLE_DEBUG("%s: open accepted barrier", *this)
            this->_accepted.open();
          break;
        case plasma::TransactionStatus::canceled:
          ELLE_DEBUG("%s: open canceled barrier", *this)
            this->_canceled.open();
          break;
        case plasma::TransactionStatus::failed:
          ELLE_DEBUG("%s: open failed barrier", *this)
            this->_failed.open();
          break;
        case plasma::TransactionStatus::finished:
          ELLE_DEBUG("%s: open finished barrier", *this)
            this->_finished.open();
          break;
        case plasma::TransactionStatus::rejected:
          ELLE_DEBUG("%s: open rejected barrier", *this)
            this->_rejected.open();
          break;
        case plasma::TransactionStatus::initialized:
        case plasma::TransactionStatus::ready:
          ELLE_DEBUG("%s: ignore status %s", *this, status);
          break;
        case plasma::TransactionStatus::created:
        case plasma::TransactionStatus::none:
        case plasma::TransactionStatus::started:
        case plasma::TransactionStatus::_count:
          elle::unreachable();
      }
    }

    void
    SendMachine::_request_network()
    {
      ELLE_TRACE_SCOPE("%s: request network", *this);
      this->current_state(TransferState_SenderCreateNetwork);

      elle::utility::Time time; time.Current();
      std::string network_name =
        elle::sprintf("%s-%s", this->peer_id(), time.nanoseconds);

      auto network_id =
        this->state().meta().create_network(network_name).created_network_id;
      this->network_id(network_id);

      this->state().reporter()[this->network_id()].store(
        "network.create.succeed",
        {{MKey::value, this->network_id()}});

      this->state().google_reporter()[this->state().me().id].store(
        "network.create.succeed");

      this->state().meta().network_add_device(
        this->network().name(), this->state().device().id);
    }

    void
    SendMachine::_create_transaction()
    {
      ELLE_TRACE_SCOPE("%s: create transaction with netowrk %s",
                       *this, this->network_id());
      this->current_state(TransferState_SenderCreateTransaction);

      auto total_size =
        [] (std::unordered_set<std::string> const& files) -> size_t
        {
          ELLE_TRACE_FUNCTION(files);

          size_t size = 0;
          {
            for (auto const& file: files)
            {
              auto _size = elle::os::file::size(file);
              ELLE_DEBUG("%s: %i", file, _size);
              size += _size;
            }
          }
          return size;
        };

      int size = total_size(this->_files);

      std::string first_file =
        boost::filesystem::path(*(this->_files.cbegin())).filename().string();

      std::list<std::string> file_list{this->_files.size()};
      std::transform(
        this->_files.begin(),
        this->_files.end(),
        file_list.begin(),
        [] (std::string const& el) {
          return boost::filesystem::path(el).filename().string();
        });
      ELLE_ASSERT_EQ(file_list.size(), this->_files.size());

      ELLE_DEBUG("create transaction");
      this->transaction_id(
        this->state().meta().create_transaction(
          this->peer_id(),
          file_list,
          this->_files.size(),
          size,
          boost::filesystem::is_directory(first_file),
          this->network().name(),
          this->state().device().id,
          this->_message
          ).created_transaction_id
        );

      ELLE_TRACE("created transaction: %s", this->transaction_id());

      // XXX: Ensure recipient is an id.

      ELLE_DEBUG("store peer id");
      this->peer_id(this->state().user(this->peer_id(), true).id);
      ELLE_TRACE("peer id: %s", this->peer_id());

      this->state().meta().network_add_user(
        this->network().name(), this->peer_id());

      auto nb = operation_detail::blocks::create(this->network().name(),
                                                 this->state().identity());

      this->state().meta().update_network(this->network().name(),
                                          nullptr,
                                          &nb.root_block,
                                          &nb.root_address,
                                          &nb.group_block,
                                          &nb.group_address);

      auto network = this->state().meta().network(this->network().name());

      this->descriptor().store(this->state().identity());

      using namespace elle::serialize;
      {
        nucleus::neutron::Object directory{
          from_string<InputBase64Archive>(network.root_block)
            };

        this->storage().store(this->descriptor().meta().root(), directory);
      }

      {
        nucleus::neutron::Group group{
          from_string<InputBase64Archive>(network.group_block)
            };

        nucleus::proton::Address group_address{
          from_string<InputBase64Archive>(network.group_address)
            };

        this->storage().store(group_address, group);
      }

      this->state().meta().update_transaction(this->transaction_id(),
                                              plasma::TransactionStatus::initialized);
    }

    void
    SendMachine::_copy_files()
    {
      ELLE_TRACE_SCOPE("%s: copy the files", *this);
      this->current_state(TransferState_SenderCopyFiles);

      nucleus::neutron::Subject subject;
      subject.Create(this->descriptor().meta().administrator_K());

      this->state().reporter()[this->transaction_id()].store(
        "transaction.preparing", this->transaction_metric());

      operation_detail::to::send(
        this->etoile(), this->descriptor(), subject, this->_files);

      this->state().reporter()[this->transaction_id()].store(
        "transaction.prepared", this->transaction_metric());
    }

    void
    SendMachine::_wait_for_accept()
    {
      ELLE_TRACE_SCOPE("%s: waiting for peer to accept or reject", *this);
      this->current_state(TransferState_SenderWaitForDecision);

      // There are two ways to go to the next step:
      // - Checking local state, meaning that during the copy, we recieved an
      //   accepted, so we can directly go the next step.
      // - Waiting for the accepted notification.
    }

    void
    SendMachine::_set_permissions()
    {
      ELLE_TRACE_SCOPE("%s: set permissions %s", *this, this->transaction_id());
      this->current_state(TransferState_GrantPermissions);

      ELLE_DEBUG("%s: peer object id %s", *this, this->peer_id());
      auto id = this->state().user_indexes().at(this->peer_id());
      ELLE_DEBUG("%s: peer id %s", *this, id);
      auto peer = this->state().users().at(id);
      ELLE_DEBUG("%s: peer is %s", *this, peer);

      auto peer_public_key = peer.public_key;

      ELLE_ASSERT_NEQ(peer_public_key.length(), 0u);

      nucleus::neutron::User::Identity public_key;
      public_key.Restore(peer_public_key);

      nucleus::neutron::Subject subject;
      subject.Create(public_key);

      ELLE_DEBUG("%s: network id %s", *this, this->network_id());
      auto network = this->state().meta().network(this->network_id());
      ELLE_DEBUG("%s: network %s", *this, network);
      auto group_address = network.group_address;

      nucleus::neutron::Group::Identity group;
      group.Restore(group_address);

      operation_detail::user::add(this->etoile(), group, subject);
      operation_detail::user::set_permissions(
        this->etoile(), subject, nucleus::neutron::permissions::write);

      this->state().meta().update_transaction(this->transaction_id(),
                                              plasma::TransactionStatus::ready);
    }

    void
    SendMachine::_transfer_operation()
    {
      // Nothing to do.
    }

    /*----------.
    | Printable |
    `----------*/

    std::string
    SendMachine::type() const
    {
      return "SendMachine";
    }

  }
}
