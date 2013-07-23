#include "SendMachine.hh"

#include "ReceiveMachine.hh"

#include <surface/gap/_detail/TransferOperations.hh>

#include <nucleus/neutron/Object.hh>

#include <lune/Descriptor.hh>
#include <hole/Passport.hh>
#include <lune/Identity.hh>

#include <elle/os/path.hh>
#include <elle/os/file.hh>

#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("surface.gap.SendMachine");

namespace surface
{
  namespace gap
  {
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
          std::bind(&SendMachine::_set_permissions, this))),
      _publish_interfaces_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_publish_interfaces, this))),
      _connection_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_connection, this))),
      _transfer_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_transfer, this))),
      _clean_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_clean, this))),
      _fail_state(
        this->_machine.state_make(
          std::bind(&SendMachine::_fail, this)))
    {
      this->_machine.transition_add(_request_network_state,
                                    _create_transaction_state);
      this->_machine.transition_add(_create_transaction_state,
                                    _copy_files_state);
      this->_machine.transition_add(_copy_files_state,
                                    _wait_for_accept_state);
      this->_machine.transition_add(_wait_for_accept_state,
                                    _set_permissions_state,
                                    reactor::Waitables{&_accepted});
      this->_machine.transition_add(_set_permissions_state,
                                    _publish_interfaces_state);

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
                                    _clean_state,
                                    reactor::Waitables{&_finished});

      // Exception handling.
      // this->_m.transition_add_catch(_request_network_state, _fail);
    }

    SendMachine::~SendMachine()
    {}

    SendMachine::SendMachine(surface::gap::State const& state,
                             std::string const& recipient,
                             std::unordered_set<std::string>&& files):
      SendMachine(state)
    {
      ELLE_LOG_SCOPE("%s: send %s to %s", *this, files, recipient);

      if (files.empty())
        throw elle::Exception("no files to send");

      std::swap(this->_files, files);

      ELLE_ASSERT_NEQ(this->_files.size(), 0u);

      this->_recipient = recipient;
      this->run();
    }

    void
    SendMachine::on_transaction_update(plasma::meta::TransactionResponse const& transaction)
    {
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
        case plasma::TransactionStatus::created:
        case plasma::TransactionStatus::initialized:
        case plasma::TransactionStatus::ready:
        case plasma::TransactionStatus::rejected:
        case plasma::TransactionStatus::_count:
          break;
      }
    }

    void
    SendMachine::on_user_update(plasma::meta::UserResponse const& user)
    {
    }

    void
    SendMachine::on_network_update(plasma::meta::NetworkResponse const& network)
    {
    }

    void
    SendMachine::_request_network()
    {
      elle::utility::Time time; time.Current();
      std::string network_name =
        elle::sprintf("%s-%s", this->_recipient, time.nanoseconds);
      std::cerr << "network_name: " << network_name << std::endl;

      this->network_id(
        this->state().meta().create_network(network_name).created_network_id);

      // this->state().reporter()[this->_network_id].store(
      //   "network.create.succeed",
      //   {{MKey::value, this->_network_id}});

      // this->_google_reporter[this->_self().id].store("network.create.succeed");

      this->state().meta().network_add_device(
        this->network().name(), this->state().device_id());
      ELLE_LOG("this->_network_id: %s", this->network().name());
    }

    void
    SendMachine::_create_transaction()
    {
      ELLE_LOG("%s", __PRETTY_FUNCTION__);
      ELLE_ASSERT_NEQ(this->network_id().length(), 0u);

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

      // Create transaction.
      this->transaction_id(this->state().meta().create_transaction(
                             this->_recipient, first_file, this->_files.size(), size,
                             boost::filesystem::is_directory(first_file), this->network().name(),
                             this->state().device_id()).created_transaction_id);

      // XXX: Ensure recipient is an id.
      this->_recipient = this->state().meta().user(this->_recipient).id;
      this->state().meta().network_add_user(
        this->network().name(), this->_recipient);

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
    }

    void
    SendMachine::_copy_files()
    {
      ELLE_LOG("%s", __PRETTY_FUNCTION__);

      nucleus::neutron::Subject subject;
      subject.Create(this->descriptor().meta().administrator_K());

      operation_detail::to::send(
        this->etoile(), this->descriptor(), subject, this->_files);
    }

    void
    SendMachine::_wait_for_accept()
    {
      ELLE_LOG("%s", __PRETTY_FUNCTION__);

      // There are two ways to go to the next step:
      // - Checking local state, meaning that during the copy, we recieved an
      //   accepted, so we can directly go the next step.
      // - Waiting for the accepted notification.
    }

    void
    SendMachine::_set_permissions()
    {
      auto peer_public_key = this->state().meta().user(this->_recipient).public_key;

      ELLE_ASSERT_NEQ(peer_public_key.length(), 0u);

      nucleus::neutron::User::Identity public_key;
      public_key.Restore(peer_public_key);

      nucleus::neutron::Subject subject;
      subject.Create(public_key);

      auto group_address = this->state().meta().network(this->network_id()).group_address;

      nucleus::neutron::Group::Identity group;
      group.Restore(group_address);

      operation_detail::user::add(this->etoile(), group, subject);
      operation_detail::user::set_permissions(
        this->etoile(), subject, nucleus::neutron::permissions::write);
    }

    void
    SendMachine::_publish_interfaces()
    {

    }

    void
    SendMachine::_connection()
    {

    }

    void
    SendMachine::_transfer()
    {

    }

    void
    SendMachine::_clean()
    {
    }

    void
    SendMachine::_fail()
    {
    }
  }
}
