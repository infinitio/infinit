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
    SendMachine::SendMachine(surface::gap::State const& state):
      TransferMachine(state),
      _request_network_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_request_network, this))),
      _create_transaction_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_create_transaction, this))),
      _copy_files_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_copy_files, this))),
      _wait_for_accept_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_wait_for_accept, this))),
      _set_permissions_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_set_permissions, this)))
    {
      this->_machine.transition_add(_request_network_state,
                                    _create_transaction_state);
      this->_machine.transition_add(_create_transaction_state,
                                    _copy_files_state);
      this->_machine.transition_add(_copy_files_state,
                                    _wait_for_accept_state);
      this->_machine.transition_add(
        _wait_for_accept_state,
        _set_permissions_state,
        reactor::Waitables{this->_accepted},
        false,
        [&] () -> bool
        {
          return this->state().transaction_manager().one(
            this->transaction_id()).status == TransactionStatus::accepted;
        }
        );

      this->_machine.transition_add(
        _wait_for_accept_state,
        _clean_state,
        reactor::Waitables{this->_rejected},
        false,
        [&] () -> bool
        {
          return this->state().transaction_manager().one(
            this->transaction_id()).status == TransactionStatus::rejected;
        }
        );

      this->_machine.transition_add(_set_permissions_state,
                                    _transfer_core_state);

      // Cancel.
      // this->_machine.transition_add(_request_network_state, _cancel_state, reactor::Waitables{this->_canceled}, true);
      // this->_machine.transition_add(_create_transaction_state, _cancel_state, reactor::Waitables{this->_canceled}, true);
      // this->_machine.transition_add(_copy_files_state, _cancel_state, reactor::Waitables{this->_canceled}, true);
      // this->_machine.transition_add(_wait_for_accept_state, _cancel_state, reactor::Waitables{this->_canceled}, true);
      // this->_machine.transition_add(_set_permissions_state, _cancel_state, reactor::Waitables{this->_canceled}, true);

      // Exception.
      this->_machine.transition_add_catch(_request_network_state, _fail_state);
      this->_machine.transition_add_catch(_create_transaction_state, _fail_state);
      this->_machine.transition_add_catch(_copy_files_state, _fail_state);
      this->_machine.transition_add_catch(_wait_for_accept_state, _fail_state);
      this->_machine.transition_add_catch(_set_permissions_state, _fail_state);
    }

    SendMachine::~SendMachine()
    {
      this->_stop();
    }

    SendMachine::SendMachine(surface::gap::State const& state,
                             std::string const& recipient,
                             std::unordered_set<std::string>&& files):
      SendMachine(state)
    {
      ELLE_TRACE_SCOPE("%s: send %s to %s", *this, files, recipient);

      if (files.empty())
        throw elle::Exception("no files to send");

      std::swap(this->_files, files);

      ELLE_ASSERT_NEQ(this->_files.size(), 0u);

      this->peer_id(recipient);
      this->run(this->_request_network_state);
    }

    SendMachine::SendMachine(surface::gap::State const& state,
                             plasma::Transaction const& transaction):
      SendMachine(state)
    {
      ELLE_TRACE_SCOPE("%s: constructing machine for transaction %s",
                       *this, transaction);

      this->transaction_id(transaction.id);
      this->network_id(transaction.network_id);
      this->peer_id(transaction.recipient_id);

      switch (transaction.status)
      {
        case plasma::TransactionStatus::initialized:
          // XXX: This is wrong. If the local copy was not over, the transfer
          // will fail. We should add a state on the server or use a local
          // journal to make sure the copy was over.
          // For the moment, none of thoose options are implemented and there
          // is no way to know if the copy was successfull.
          this->run(this->_wait_for_accept_state);
          break;
        case plasma::TransactionStatus::accepted:
          this->run(this->_set_permissions_state);
          break;
        case plasma::TransactionStatus::ready:
          this->run(this->_transfer_core_state);
          break;
        case plasma::TransactionStatus::finished:
          this->run(this->_finish_state);
          break;
        case plasma::TransactionStatus::canceled:
          this->run(this->_cancel_state);
          break;
        case plasma::TransactionStatus::failed:
          this->run(this->_fail_state);
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

    void
    SendMachine::on_transaction_update(plasma::Transaction const& transaction)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction %s",
                       *this, transaction);

      ELLE_ASSERT_EQ(this->transaction_id(), transaction.id);
      switch (transaction.status)
      {
        case plasma::TransactionStatus::accepted:
          this->_accepted.signal();
          break;
        case plasma::TransactionStatus::canceled:
          this->_canceled.signal();
          break;
        case plasma::TransactionStatus::failed:
          this->_failed.signal();
          break;
        case plasma::TransactionStatus::finished:
          this->_finished.signal();
          break;
        case plasma::TransactionStatus::rejected:
          this->_rejected.signal();
          break;
        case plasma::TransactionStatus::initialized:
        case plasma::TransactionStatus::ready:
          break;
        case plasma::TransactionStatus::created:
        case plasma::TransactionStatus::none:
        case plasma::TransactionStatus::started:
        case plasma::TransactionStatus::_count:
          elle::unreachable();
      }
    }

    void
    SendMachine::on_peer_connection_update(PeerConnectionUpdateNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: update with new peer connection status %s",
                       *this, notif);

      ELLE_ASSERT_EQ(this->network_id(), notif.network_id);

      if (notif.status)
        this->_peer_online.signal();
      else
        this->_peer_offline.signal();
    }

    void
    SendMachine::_request_network()
    {
      ELLE_TRACE_SCOPE("%s: request network", *this);

      elle::utility::Time time; time.Current();
      std::string network_name =
        elle::sprintf("%s-%s", this->peer_id(), time.nanoseconds);

      this->network_id(
        this->state().meta().create_network(network_name).created_network_id);

      // this->state().reporter()[this->_network_id].store(
      //   "network.create.succeed",
      //   {{MKey::value, this->_network_id}});

      // this->_google_reporter[this->_self().id].store("network.create.succeed");

      this->state().meta().network_add_device(
        this->network().name(), this->state().device_id());
    }

    void
    SendMachine::_create_transaction()
    {
      ELLE_TRACE_SCOPE("%s: create transaction with netowrk %s",
                       *this, this->network_id());

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

      ELLE_DEBUG("create transaction");
      this->transaction_id(
        this->state().meta().create_transaction(
          this->peer_id(), first_file, this->_files.size(), size,
          boost::filesystem::is_directory(first_file), this->network().name(),
          this->state().device_id()).created_transaction_id);

      ELLE_TRACE("created transaction: %s", this->transaction_id());

      // XXX: Ensure recipient is an id.

      ELLE_DEBUG("store peer id");
      this->peer_id(this->state().meta().user(this->peer_id()).id);
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

      nucleus::neutron::Subject subject;
      subject.Create(this->descriptor().meta().administrator_K());

      operation_detail::to::send(
        this->etoile(), this->descriptor(), subject, this->_files);
    }

    void
    SendMachine::_wait_for_accept()
    {
      ELLE_TRACE_SCOPE("%s: waiting for peer to accept or reject", *this);

      // There are two ways to go to the next step:
      // - Checking local state, meaning that during the copy, we recieved an
      //   accepted, so we can directly go the next step.
      // - Waiting for the accepted notification.
    }

    void
    SendMachine::_set_permissions()
    {
      ELLE_TRACE_SCOPE("%s: set permissions %s", *this, this->transaction_id());

      auto peer_public_key = this->state().user_manager().one(this->peer_id()).public_key;

      ELLE_ASSERT_NEQ(peer_public_key.length(), 0u);

      nucleus::neutron::User::Identity public_key;
      public_key.Restore(peer_public_key);

      nucleus::neutron::Subject subject;
      subject.Create(public_key);

      auto group_address = this->state().network_manager().one(this->network_id()).group_address;

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
