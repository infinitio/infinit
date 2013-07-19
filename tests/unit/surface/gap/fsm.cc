#include <surface/gap/State.hh>

#include <plasma/meta/Client.hh>

#include <reactor/fsm.hh>
#include <reactor/scheduler.hh>
#include <reactor/thread.hh>
#include <reactor/waitable.hh>

#include <elle/os/file.hh>
#include <elle/log.hh>

#include <boost/filesystem.hpp>

#include <functional>
#include <string>
#include <unordered_set>

ELLE_LOG_COMPONENT("a");

inline
reactor::Scheduler&
sched()
{
  static reactor::Scheduler scheduler;
  return scheduler;
}

namespace surface
{
  namespace gap
  {

    struct TransferMachine
    {
      TransferMachine()
      {}

      virtual
      ~TransferMachine() = 0;

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
    protected:
      std::unique_ptr<reactor::Thread> _run;
      reactor::fsm::Machine _m;
    };

    TransferMachine::~TransferMachine()
    {}

    struct SendingMachine:
      public TransferMachine
    {
    private:
      void
      _request_network()
      {
        std::string network_name =
          elle::sprintf("%s-%s",
                        this->_recipient, 0);
//                        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now()).count())

        this->_network_id =
          this->_state.meta().create_network(network_name).created_network_id;

        // this->_state.reporter()[this->_network_id].store(
        //   "network.create.succeed",
        //   {{MKey::value, this->_network_id}});

        // this->_google_reporter[this->_self().id].store("network.create.succeed");

        this->_state.meta().network_add_device(
          this->_network_id, this->_state.device_id());
      }

      void
      _create_transaction()
      {
        ELLE_ASSERT_NEQ(this->_network_id.length(), 0u);

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

        this->_transaction_id = this->_state.meta().create_transaction(
          this->_recipient, first_file, files.size(), size,
          boost::filesystem::::is_directory(first_file), this->_network_id,
          this->_state.device().id).created_transaction_id;
      }

      void
      _copy_files()
      {
      }

      void
      _wait_for_accept()
      {
        // There are two ways to go to the next step:
        // - Checking local state, meaning that during the copy, we recieved an
        //   accepted, so we can directly go the next step.
        // - Waiting for the accepted notification.
      }

      void
      _set_permissions()
      {
      }

      void
      _publish_interfaces()
      {
        // Publish.
      }

      void
      _connection()
      {

      }

      void
      _upload()
      {
      }

      void
      _clean()
      {
      }

      void
      _fail()
      {
      }

    private:
      SendingMachine(surface::gap::State & state):
        _state(state),
        _request_network_state(this->_m.state_make(std::bind(&SendingMachine::_request_network, this))),
        _create_transaction_state(this->_m.state_make(std::bind(&SendingMachine::_create_transaction, this))),
        _copy_files_state(this->_m.state_make(std::bind(&SendingMachine::_copy_files, this))),
        _wait_for_accept_state(this->_m.state_make(std::bind(&SendingMachine::_wait_for_accept, this))),
        _set_permissions_state(this->_m.state_make(std::bind(&SendingMachine::_set_permissions, this))),
        _publish_interfaces_state(this->_m.state_make(std::bind(&SendingMachine::_publish_interfaces, this))),
        _connection_state(this->_m.state_make(std::bind(&SendingMachine::_connection, this))),
        _upload_state(this->_m.state_make(std::bind(&SendingMachine::_upload, this))),
        _clean_state(this->_m.state_make(std::bind(&SendingMachine::_clean, this))),
        _fail_state(this->_m.state_make(std::bind(&SendingMachine::_fail, this)))
      {

        this->_m.transition_add(_request_network_state, _create_transaction_state);
        this->_m.transition_add(_create_transaction_state, _copy_files_state);
        this->_m.transition_add(_copy_files_state, _wait_for_accept_state);
        this->_m.transition_add(_wait_for_accept_state, _set_permissions_state, reactor::Waitables{&_accepted});
        this->_m.transition_add(_set_permissions_state, _publish_interfaces_state);

        this->_m.transition_add(_publish_interfaces_state, _connection_state, reactor::Waitables{&_peer_online});
        this->_m.transition_add(_connection_state, _publish_interfaces_state, reactor::Waitables{&_peer_offline});

        this->_m.transition_add(_connection_state, _connection_state, reactor::Waitables{&_peer_online});

        this->_m.transition_add(_connection_state, _upload_state, reactor::Waitables{&_peer_connected});
        this->_m.transition_add(_upload_state, _connection_state, reactor::Waitables{&_peer_disconnected});

        this->_m.transition_add(_upload_state, _clean_state, reactor::Waitables{&_finished});

        // Exception handling.
        // this->_m.transition_add_catch(_request_network_state, _fail);
      }

    public:
      virtual
      ~SendingMachine()
      {}

    public:
      SendingMachine(surface::gap::State& state,
                     std::string const& recipient,
                     std::unordered_set<std::string> files):
        SendingMachine(state)
      {
        ELLE_TRACE_SCOPE("%s: send %s to %s", *this, files, recipient);

        _files = std::move(files);
        _recipient = recipient;


        if (files.empty())
          throw elle::Exception("no files to send");

        this->_run.reset(
          new reactor::Thread(
            sched(), "run", std::bind(&reactor::fsm::Machine::run, &this->_m)));
      }

    public:
      void
      on_transaction_update(plasma::meta::TransactionResponse const& transaction)
      {
        ELLE_ASSERT_EQ(this->_transaction_id, transaction.id);
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
            this->_failed.signal();
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
      on_user_update(plasma::meta::UserResponse const& user)
      {
      }

      void
      on_network_update(plasma::meta::NetworkResponse const& network)
      {

      }

      ELLE_ATTRIBUTE(surface::gap::State&, state);
      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, request_network_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, create_transaction_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, copy_files_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_accept_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, set_permissions_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, publish_interfaces_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, connection_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, upload_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, clean_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, fail_state);

      // User status signal.
      ELLE_ATTRIBUTE(reactor::Signal, peer_online);
      ELLE_ATTRIBUTE(reactor::Signal, peer_offline);

      // Slug signal.
      ELLE_ATTRIBUTE(reactor::Signal, peer_connected);
      ELLE_ATTRIBUTE(reactor::Signal, peer_disconnected);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Signal, accepted);
      ELLE_ATTRIBUTE(reactor::Signal, finished);
      ELLE_ATTRIBUTE(reactor::Signal, canceled);
      ELLE_ATTRIBUTE(reactor::Signal, failed);


      /*-----------------.
      | Transaction data |
      `-----------------*/
      ELLE_ATTRIBUTE(std::string, recipient);
      ELLE_ATTRIBUTE(std::unordered_set<std::string>, files);
      ELLE_ATTRIBUTE(std::string, network_id);
      ELLE_ATTRIBUTE(std::string, transaction_id);
    };
  }
}

int
main(void)
{
  surface::gap::State state;
  state.register_("dim", "dimrok@infinit.io", std::string(64, 'c'), "bitebite");

  surface::gap::SendingMachine sm{state, "bite", {}};

  // reactor::Thread trigger_connected(sched(), "connected", [&] () {
  //     auto& current = *sched().current();
  //     reactor::Sleep s(sched(), 3s);
  //     s.run();
  //     sm.on_transaction_update(true);
  //   });

  sched().run();
}
