#include <surface/gap/TransferMachine.hh>
#include <surface/gap/_detail/TransferOperations.hh>
#include <surface/gap/Rounds.hh>
#include <surface/gap/State.hh>
#include <metrics/Metric.hh>

#include <common/common.hh>

#include <papier/Descriptor.hh>
#include <papier/Authority.hh>

#include <etoile/Etoile.hh>

#include <reactor/fsm/Machine.hh>
#include <reactor/exception.hh>

#include <cryptography/oneway.hh>

#include <elle/os/getenv.hh>
#include <elle/network/Interface.hh>
#include <elle/printf.hh>
#include <elle/serialize/insert.hh>

#include <functional>

ELLE_LOG_COMPONENT("surface.gap.TransferMachine");

namespace surface
{
  namespace gap
  {
    Notification::Type TransferMachine::Notification::type = NotificationType_TransactionUpdate;

    TransferMachine::Notification::Notification(uint32_t id,
                                                TransferState status):
      id(id),
      status(status)
    {}

    TransferMachine::Snapshot::Snapshot(Data const& data,
                                        TransferState const state,
                                        std::unordered_set<std::string> const& files,
                                        std::string const& message):
      data(data),
      state(state),
      files(files),
      message(message)
    {}

    //---------- TransferMachine -----------------------------------------------
    TransferMachine::TransferMachine(surface::gap::State const& state,
                                     uint32_t id,
                                     std::shared_ptr<TransferMachine::Data> data):
      _snapshot_path(
        boost::filesystem::path(
          common::infinit::transaction_snapshots_directory(state.me().id) /
          boost::filesystem::unique_path()).string()),
      _id(id),
      _machine(),
      _machine_thread(),
      _transfer_core_state(
        this->_machine.state_make(
          "transfer core", std::bind(&TransferMachine::_transfer_core, this))),
      _finish_state(
        this->_machine.state_make(
          "finish", std::bind(&TransferMachine::_finish, this))),
      _reject_state(
        this->_machine.state_make(
          "reject", std::bind(&TransferMachine::_reject, this))),
      _cancel_state(
        this->_machine.state_make(
          "cancel", std::bind(&TransferMachine::_cancel, this))),
      _fail_state(
        this->_machine.state_make(
          "fail", std::bind(&TransferMachine::_fail, this))),
      _remote_clean_state(
        this->_machine.state_make(
          "remote clean", std::bind(&TransferMachine::_remote_clean, this))),
      _local_clean_state(
        this->_machine.state_make(
          "local clean", std::bind(&TransferMachine::_local_clean, this))),
      _core_machine(),
      _core_machine_thread(),
      _publish_interfaces_state(
        this->_core_machine.state_make(
          "publish interfaces", std::bind(&TransferMachine::_publish_interfaces, this))),
      _connection_state(
        this->_core_machine.state_make(
          "connection", std::bind(&TransferMachine::_connection, this))),
      _wait_for_peer_state(
        this->_core_machine.state_make(
          "wait for peer", std::bind(&TransferMachine::_wait_for_peer, this))),
      _transfer_state(
        this->_core_machine.state_make(
          "transfer", std::bind(&TransferMachine::_transfer, this))),
      _core_stoped_state(
        this->_core_machine.state_make(
          "core stoped", std::bind(&TransferMachine::_core_stoped, this))),
      _core_failed_state(
        this->_core_machine.state_make(
          "core failed", std::bind(&TransferMachine::_core_failed, this))),
      _core_paused_state(
        this->_core_machine.state_make(
          "core paused", std::bind(&TransferMachine::_core_paused, this))),
      _peer_online(),
      _peer_offline(),
      _peer_connected(),
      _peer_disconnected(),
      _progress(0.0f),
      _progress_mutex(),
      _pull_progress_thread(),
      _state(state),
      _data(std::move(data))
    {
      ELLE_TRACE_SCOPE("%s: creating transfer machine: %s", *this, this->_data);

      // Normal way.
      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_finish_state,
                                    reactor::Waitables{&this->_finished});
      this->_machine.transition_add(this->_finish_state,
                                    this->_remote_clean_state);

      // Cancel way.
      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(this->_cancel_state,
                                    this->_remote_clean_state);

      // Fail way.
      this->_machine.transition_add_catch(this->_transfer_core_state,
                                          this->_fail_state);

      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_fail_state,
                                    reactor::Waitables{&this->_failed}, true);

      this->_machine.transition_add(this->_fail_state,
                                    this->_remote_clean_state);
      // Reject.
      this->_machine.transition_add(this->_reject_state,
                                    this->_remote_clean_state);

      // Clean.
      this->_machine.transition_add(this->_remote_clean_state,
                                    this->_local_clean_state);

      auto& end_state = this->_machine.state_make(
        [this]
        {
          ELLE_TRACE_SCOPE("%s: end of machine", *this);
        });

      this->_machine.transition_add(this->_local_clean_state, end_state);

      // XXX: Create a temporary state non handled exception on final states.
      // If something went wrong like a connection troubles, we don't want an
      // exception
      // to escape into scheduler.
      // A better way to handle this could be a retry state.
      // If the exception is not too critical, we could restart in the last
      // state.
      auto& err = this->_machine.state_make(
        [this]
        {
          ELLE_ERR("%s: something went wrong while cleaning: escaping", *this);
        });

      this->_machine.transition_add_catch(this->_fail_state, err);
      this->_machine.transition_add_catch(this->_cancel_state, err);
      this->_machine.transition_add_catch(this->_finish_state, err);
      this->_machine.transition_add_catch(this->_remote_clean_state, err);
      this->_machine.transition_add_catch(this->_local_clean_state, err);



      /*-------------.
      | Core Machine |
      `-------------*/
      this->_core_machine.transition_add(this->_publish_interfaces_state,
                                         this->_connection_state,
                                         reactor::Waitables{&this->_peer_online});
      this->_core_machine.transition_add(this->_connection_state,
                                         this->_wait_for_peer_state,
                                         reactor::Waitables{&this->_peer_offline});
      this->_core_machine.transition_add(this->_wait_for_peer_state,
                                         this->_connection_state,
                                         reactor::Waitables{&this->_peer_online});
      this->_core_machine.transition_add(this->_connection_state,
                                         this->_connection_state,
                                         reactor::Waitables{&this->_peer_online});

      this->_core_machine.transition_add(
        this->_publish_interfaces_state,
        this->_core_stoped_state,
        reactor::Waitables{&this->_canceled}, true);
      this->_core_machine.transition_add(
        this->_connection_state,
        this->_core_stoped_state,
        reactor::Waitables{&this->_canceled}, true);
      this->_core_machine.transition_add(
        this->_wait_for_peer_state,
        this->_core_stoped_state,
        reactor::Waitables{&this->_canceled}, true);
      this->_core_machine.transition_add(
        this->_transfer_state,
        this->_core_stoped_state,
        reactor::Waitables{&this->_canceled}, true);

      this->_core_machine.transition_add(
        this->_connection_state, this->_transfer_state);
      this->_core_machine.transition_add(
        this->_transfer_state, this->_core_stoped_state);

      this->_core_machine.transition_add_catch(this->_publish_interfaces_state,
                                               this->_core_failed_state);
      this->_core_machine.transition_add_catch(this->_wait_for_peer_state,
                                               this->_core_failed_state);
      this->_core_machine.transition_add_catch(this->_connection_state,
                                               this->_core_failed_state);
      this->_core_machine.transition_add_catch(this->_transfer_state,
                                               this->_core_failed_state);
    }

    TransferMachine::~TransferMachine()
    {
      ELLE_TRACE_SCOPE("%s: destroying transfer machine", *this);
    }

    TransferMachine::Snapshot
    TransferMachine::_make_snapshot() const
    {
      return Snapshot{*this->data(), this->_current_state};
    }

    void
    TransferMachine::_save_snapshot() const
    {
      elle::serialize::to_file(this->_snapshot_path.string()) << this->_make_snapshot();
    }

    void
    TransferMachine::current_state(TransferState const& state)
    {
      this->_current_state = state;
      this->_save_snapshot();
      this->state().enqueue(Notification(this->id(), state));
    }

    void
    TransferMachine::_transfer_core()
    {
      ELLE_TRACE_SCOPE("%s: start transfer core machine", *this);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      reactor::Scheduler& scheduler = *reactor::Scheduler::scheduler();

      this->_core_machine_thread.reset(
        new reactor::Thread{
          scheduler,
          "run core",
          [&]
          {
            try
            {
              this->_core_machine.run();
              ELLE_TRACE("%s: core machine finished properly", *this);
            }
            catch (std::exception const&)
            {
              ELLE_ERR("%s: something went wrong while transfering", *this);
            }
          }});
      scheduler.current()->wait(*this->_core_machine_thread);

      if (this->_failed.opened())
        throw Exception(gap_error, "an error occured");

      ELLE_DEBUG("%s: transfer core finished", *this);
    }

    void
    TransferMachine::_local_clean()
    {
      ELLE_TRACE_SCOPE("%s: clean local %s", *this, this->data()->network_id);
      this->current_state(TransferState_CleanLocal);

      if (!this->data()->network_id.empty())
      {
        auto path = common::infinit::network_directory(
          this->state().me().id, this->network_id());

        if (boost::filesystem::exists(path))
        {
          ELLE_DEBUG("%s: remove network at %s", *this, path);

          boost::filesystem::remove_all(path);
        }
      }

      ELLE_DEBUG("finalize etoile")
        this->_etoile.reset();
      ELLE_DEBUG("finalize hole")
        this->_hole.reset();
      ELLE_DEBUG("%s: local state successfully cleaned", *this);
    }

    void
    TransferMachine::_remote_clean()
    {
      ELLE_TRACE_SCOPE("%s: clean remote network %s",
                       *this, this->data()->network_id);
      this->current_state(TransferState_CleanRemote);

      if (!this->data()->network_id.empty())
      {
        try
        {
          this->state().meta().delete_network(this->network_id());
        }
        catch (reactor::Terminate const&)
        {
          ELLE_TRACE("%s: machine terminated before deleting network: %s",
                     *this, elle::exception_string());
          throw;
        }
        catch (std::exception const&)
        {
          ELLE_ERR("%s: clean failed: network %s wasn't deleted: %s",
                   *this, this->network_id(), elle::exception_string());
        }
      }

      ELLE_DEBUG("%s: cleaned", *this);
    }

    void
    TransferMachine::_finish()
    {
      ELLE_TRACE_SCOPE("%s: machine finished", *this);
      this->current_state(TransferState_Finished);
      this->_finalize(plasma::TransactionStatus::finished);
      ELLE_DEBUG("%s: finished", *this);
    }

    void
    TransferMachine::_reject()
    {
      ELLE_TRACE_SCOPE("%s: machine rejected", *this);
      this->current_state(TransferState_Rejected);
      this->_finalize(plasma::TransactionStatus::rejected);
      ELLE_DEBUG("%s: rejected", *this);
    }

    void
    TransferMachine::_cancel()
    {
      ELLE_TRACE_SCOPE("%s: machine canceled", *this);
      this->current_state(TransferState_Canceled);
      this->_finalize(plasma::TransactionStatus::canceled);
      ELLE_DEBUG("%s: canceled", *this);
    }

    void
    TransferMachine::_fail()
    {
      ELLE_TRACE_SCOPE("%s: machine failed", *this);
      this->current_state(TransferState_Failed);
      this->_finalize(plasma::TransactionStatus::failed);
      ELLE_DEBUG("%s: failed", *this);
    }

    void
    TransferMachine::_finalize(plasma::TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: finalize machine: %s", *this, status);

      try
      {
        if (this->_machine.exception() != std::exception_ptr{})
          throw this->_machine.exception();
      }
      catch (reactor::Terminate const&)
      {
        ELLE_TRACE("%s: machine terminated cleanly: %s", *this, elle::exception_string());
        throw;
      }
      catch (...)
      {
        ELLE_ERR("%s: transaction failed: %s", *this, elle::exception_string());
      }

      if (!this->_data->empty())
      {
        try
        {
          this->state().meta().update_transaction(
            this->transaction_id(), status);
        }
        catch (reactor::Terminate const&)
        {
          ELLE_TRACE("%s: machine terminated while canceling transaction: %s",
                     *this, elle::exception_string());
          throw;
        }
        catch (plasma::meta::Exception const& e)
        {
          if (e.err == plasma::meta::Error::transaction_already_finalized)
            ELLE_TRACE("%s: transaction already finalized", *this);
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
        ELLE_DEBUG("%s: transaction id is still empty", *this);
      }

      if (!this->network_id().empty())
      {
        try
        {
          this->state().meta().delete_network(this->network_id());
        }
        catch (reactor::Terminate const&)
        {
          ELLE_TRACE("%s: machine terminated while deleting netork: %s",
                     *this, elle::exception_string());
          throw;
        }
        catch (std::exception const&)
        {
          ELLE_ERR("%s: deleting network %s failed: %s",
                   *this, this->network_id(), elle::exception_string());
        }
      }
      else
      {
        ELLE_DEBUG("%s: network id is still empty", *this);
      }
      ELLE_DEBUG("%s: finalized", *this);
    }

    void
    TransferMachine::_run(reactor::fsm::State& initial_state)
    {
      ELLE_TRACE_SCOPE("%s: running transfer machine", *this);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      auto& scheduler = *reactor::Scheduler::scheduler();

      this->_machine_thread.reset(
        new reactor::Thread{
          scheduler,
          "run",
          [&]
          {
            this->_machine.run(initial_state);
            ELLE_TRACE("%s: machine finished properly", *this);
            boost::filesystem::remove(this->_snapshot_path);
          }});
    }

    void
    TransferMachine::peer_connection_update(bool user_status)
    {
      ELLE_TRACE_SCOPE("%s: update with new peer connection status %s",
                       *this, user_status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      if (user_status)
      {
        ELLE_DEBUG("%s: signal peer online", *this)
          this->_peer_online.signal();
      }
      else
      {
        ELLE_DEBUG("%s: signal peer offline", *this)
          this->_peer_offline.signal();
      }
    }

    void
    TransferMachine::cancel()
    {
      ELLE_TRACE_SCOPE("%s: cancel transaction %s", *this, this->data()->id);
      this->_canceled.open();
    }

    bool
    TransferMachine::concerns_network(std::string const& network_id)
    {
      return this->_data->network_id == network_id;
    }

    bool
    TransferMachine::concerns_transaction(std::string const& transaction_id)
    {
      return this->_data->id == transaction_id;
    }

    bool
    TransferMachine::concerns_user(std::string const& user_id)
    {
      return (user_id == this->_data->sender_id) ||
             (user_id == this->_data->recipient_id);
    }

    bool
    TransferMachine::concerns_device(std::string const& device_id)
    {
      return (device_id == this->_data->sender_device_id) ||
             (device_id == this->_data->recipient_device_id);
    }

    bool
    TransferMachine::has_id(uint32_t id)
    {
      return (id == this->id());
    }

    void
    TransferMachine::join()
    {
      ELLE_TRACE_SCOPE("%s: join machine", *this);

      if (this->_machine_thread == nullptr)
      {
        ELLE_WARN("%s: thread already destroyed", *this);
        return;
      }

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      reactor::Thread* current = reactor::Scheduler::scheduler()->current();
      ELLE_ASSERT(current != nullptr);
      ELLE_DEBUG("%s: start joining", *this);
      if (this->_machine_thread.get() != nullptr)
      {
        current->wait(*this->_machine_thread.get());
      }
      ELLE_DEBUG("%s: successfully joined", *this);
    }

    void
    TransferMachine::_stop()
    {
      ELLE_TRACE_SCOPE("%s: stop machine for transaction %s",
                       *this, this->network_id());

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      ELLE_DEBUG("%s: finalize etoile", *this)
        this->_etoile.reset();
      ELLE_DEBUG("%s: finalize hole", *this)
        this->_hole.reset();

      if (this->_core_machine_thread != nullptr)
      {
        ELLE_DEBUG("%s: terminate machine thread", *this)
          this->_core_machine_thread->terminate_now();
        this->_core_machine_thread.reset();
      }

      if (this->_machine_thread != nullptr)
      {
        ELLE_DEBUG("%s: terminate machine thread", *this)
          this->_machine_thread->terminate_now();
        this->_machine_thread.reset();
      }
    }

    /*-------------.
    | Core Machine |
    `-------------*/
    void
    TransferMachine::_publish_interfaces()
    {
      ELLE_TRACE_SCOPE("%s: publish interfaces", *this);
      this->current_state(TransferState_PublishInterfaces);
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
      ELLE_DEBUG("%s: interfaces published", *this);
    }

    void
    TransferMachine::_connection()
    {
      ELLE_TRACE_SCOPE("%s: connecting peers", *this);
      this->current_state(TransferState_Connect);

      auto const& transaction = *this->data();

      auto endpoints = this->state().meta().device_endpoints(
        this->network_id(),
        is_sender() ? transaction.sender_device_id : transaction.recipient_device_id,
        is_sender() ? transaction.recipient_device_id : transaction.sender_device_id);

      static auto print = [] (std::string const &s) { ELLE_DEBUG("-- %s", s); };

      ELLE_DEBUG("locals")
        std::for_each(begin(endpoints.locals), end(endpoints.locals), print);
      ELLE_DEBUG("externals")
        std::for_each(begin(endpoints.externals), end(endpoints.externals), print);
      ELLE_DEBUG("fallback")
        std::for_each(begin(endpoints.fallback), end(endpoints.fallback), print);

      std::vector<std::unique_ptr<Round>> addresses;

      addresses.emplace_back(new AddressRound("local", std::move(endpoints.locals)));

      // XXX: This MUST be done before inserting fallback, cause endpoints() is
      // lazy. If you try to display endpoints for fallback, it will enable the
      // connection.
      ELLE_TRACE("%s: selected addresses (%s):", *this, addresses.size())
        for (auto& r: addresses)
          ELLE_TRACE("-- %s", r->endpoints());

      if (!endpoints.fallback.empty())
      {
        std::vector<std::string> splited;
        boost::split(splited, *std::begin(endpoints.fallback), boost::is_any_of(":"));
        ELLE_ASSERT_EQ(splited.size(), 2u);

        std::string host = splited[0];
        int port = std::stoi(splited[1]);
        addresses.emplace_back(new FallbackRound("fallback", host, port, this->network_id()));
      }

      size_t tries = 0;
      for (auto const& r: addresses)
      {
        ELLE_DEBUG("%s: round(%s): %s", *this, tries, r->name());
        ++tries;
        bool succeed = false;
        std::vector<std::unique_ptr<reactor::Thread>> connection_threads;

        for (std::string const& endpoint: r->endpoints())
        {
          ELLE_DEBUG("%s: endpoint %s", *this, endpoint);
          auto fn = [this, endpoint]
          {
            auto slug_connect = [&] (std::string const& endpoint)
            {
              std::vector<std::string> result;
              boost::split(result, endpoint, boost::is_any_of(":"));

              auto const &ip = result[0];
              auto const &port = result[1];
              ELLE_DEBUG("slug_connect(%s, %s)", ip, port)
              this->hole().portal_connect(ip, std::stoi(port), is_sender());
            };

            namespace slug = hole::implementations::slug;
            try
            {
              slug_connect(endpoint);
              ELLE_LOG("%s: connection to %s succeed", *this, endpoint);
            }
            catch (slug::AlreadyConnected const&)
            {
              ELLE_LOG("%s: connection to %s succeed (we're already connected)",
                       *this, endpoint);
            }
            catch (reactor::Terminate const&)
            {
              throw ;
            }
            catch (std::exception const& e)
            {
              ELLE_WARN("%s: connection to %s failed: %s", *this,
                        endpoint, elle::exception_string());
            }
          };

          ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

          auto& scheduler = *reactor::Scheduler::scheduler();

          std::unique_ptr<reactor::Thread> thread_ptr{
            new reactor::Thread (scheduler,
                                 elle::sprintf("connect_try(%s)", endpoint),
                                 fn)};
          connection_threads.push_back(std::move(thread_ptr));
        }

        elle::Finally _cleanup{
          [&]
          {
            for (auto& thread: connection_threads)
              thread->terminate_now();
          }
        };

        if (this->hole().hosts().empty())
        {
          ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

          auto& scheduler = *reactor::Scheduler::scheduler();

          ELLE_ASSERT(scheduler.current() != nullptr);
          auto& this_thread = *scheduler.current();

          ELLE_DEBUG("waiting for new host");
          succeed = this_thread.wait(this->hole().new_connected_host(), 10_sec);
          ELLE_DEBUG("finished waiting for new host");
        }
        else
        {
          succeed = true;
        }

        if (succeed)
        {
          // Connection successful
          ELLE_TRACE("connection round(%s) successful", r->endpoints());
          return;
        }
        else if (not succeed)
        {
          // Connection failed
          ELLE_TRACE("connection round(%s) failed/timeout", r->endpoints());
          continue;
        }
      }
      ELLE_DEBUG("%s: peers connected", *this);
    }

    void
    TransferMachine::_wait_for_peer()
    {
      ELLE_TRACE_SCOPE("%s: waiting for peer to reconnect", *this);
      this->current_state(TransferState_PeerDisconnected);
    }

    void
    TransferMachine::_transfer()
    {
      ELLE_TRACE_SCOPE("%s: start transfer operation", *this);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      reactor::Scheduler& sched = *reactor::Scheduler::scheduler();
      this->current_state(TransferState_Transfer);
      this->_pull_progress_thread.reset(
        sched.every(
          [&] () { this->_retrieve_progress(); },
          "pull progress",
          boost::posix_time::milliseconds(300)));

      elle::Finally kill_progress(
        [&] ()
        {
          this->_pull_progress_thread->terminate_now();
          this->_pull_progress_thread.reset();
        });
      this->_transfer_operation();

      ELLE_TRACE_SCOPE("%s: end of transfer operation", *this);
    }

    void
    TransferMachine::_core_stoped()
    {
      ELLE_TRACE_SCOPE("%s: core machine stoped", *this);
    }

    void
    TransferMachine::_core_failed()
    {
      ELLE_TRACE_SCOPE("%s: core machine failed", *this);
      this->_failed.open();
    }

    void
    TransferMachine::_core_paused()
    {
      ELLE_TRACE_SCOPE("%s: core machine paused", *this);
    }

    /*-----------.
    | Attributes |
    `-----------*/
    std::string const&
    TransferMachine::transaction_id() const
    {
      ELLE_ASSERT(!this->_data->id.empty());
      return this->_data->id;
    }

    void
    TransferMachine::transaction_id(std::string const& id)
    {
      if (!this->_data->id.empty())
      {
        ELLE_ASSERT_EQ(this->_data->id, id);
        return;
      }

      this->_data->id = id;
    }

    std::string const&
    TransferMachine::network_id() const
    {
      ELLE_ASSERT(!this->_data->network_id.empty());
      return this->_data->network_id;
    }

    void
    TransferMachine::network_id(std::string const& id)
    {
      if (!this->_data->network_id.empty())
      {
        ELLE_ASSERT_EQ(this->_data->network_id, id);
        return;
      }

      this->_data->network_id = id;
    }

    std::string const&
    TransferMachine::peer_id() const
    {
      if (this->is_sender())
      {
        ELLE_ASSERT(!this->_data->recipient_id.empty());
        return this->_data->recipient_id;
      }
      else
      {
        ELLE_ASSERT(!this->_data->sender_id.empty());
        return this->_data->sender_id;
      }
    }

    void
    TransferMachine::peer_id(std::string const& id)
    {
      if (this->is_sender())
      {
        if (!this->_data->recipient_id.empty() && this->_data->recipient_id != id)
          ELLE_WARN("%s: replace recipient id %s by %s",
                    *this, this->_data->recipient_id, id);
        this->_data->recipient_id = id;
      }
      else
      {
        if (!this->_data->sender_id.empty() && this->_data->sender_id != id)
          ELLE_WARN("%s: replace sender id %s by %s",
                    *this, this->_data->sender_id, id);
        this->_data->sender_id = id;
      }
    }

    nucleus::proton::Network&
    TransferMachine::network()
    {
      if (!this->_network)
      {
        ELLE_ASSERT(!this->_data->network_id.empty());
        this->_network.reset(
          new nucleus::proton::Network(this->_data->network_id));
      }

      ELLE_ASSERT(this->_network != nullptr);
      ELLE_ASSERT_EQ(this->_network->name(), this->_data->network_id);
      return *this->_network;
    }

    papier::Descriptor const&
    TransferMachine::descriptor()
    {
      ELLE_TRACE_SCOPE("%s: get descriptor", *this);
      if (!this->_descriptor)
      {
        ELLE_DEBUG_SCOPE("building descriptor");
        using namespace elle::serialize;
        std::string descriptor =
          this->state().meta().network(this->network_id()).descriptor; //network_manager().one(this->network_id()).descriptor;

        ELLE_ASSERT_NEQ(descriptor.length(), 0u);

        this->_descriptor.reset(
          new papier::Descriptor(from_string<InputBase64Archive>(descriptor)));
      }
      ELLE_ASSERT(this->_descriptor != nullptr);
      return *this->_descriptor;
    }

    hole::storage::Directory&
    TransferMachine::storage()
    {
      ELLE_TRACE_SCOPE("%s: get storage", *this);
      if (!this->_storage)
      {
        ELLE_DEBUG_SCOPE("building storage");
        this->_storage.reset(
          new hole::storage::Directory(
            this->network().name(),
            common::infinit::network_shelter(this->state().me().id,
                                             this->network().name())));
      }
      ELLE_ASSERT(this->_storage != nullptr);
      return *this->_storage;
    }

    hole::implementations::slug::Slug&
    TransferMachine::hole()
    {
      ELLE_TRACE_SCOPE("%s: get hole", *this);
      if (!this->_hole)
      {
        ELLE_DEBUG_SCOPE("building hole");
        this->_hole.reset(
          new hole::implementations::slug::Slug(
          this->storage(), this->state().passport(), papier::authority(),
            reactor::network::Protocol::tcp));
      }
      ELLE_ASSERT(this->_hole != nullptr);
      return *this->_hole;
    }

    etoile::Etoile&
    TransferMachine::etoile()
    {
      ELLE_TRACE_SCOPE("%s: get etoile", *this);
      if (!this->_etoile)
      {
        ELLE_DEBUG_SCOPE("building descriptor");
        this->_etoile.reset(
          new etoile::Etoile(this->state().identity().pair(),
                             &(this->hole()),
                             this->descriptor().meta().root()));
      }
      ELLE_ASSERT(this->_etoile != nullptr);
      return *this->_etoile;
    }

    void
    TransferMachine::_retrieve_progress()
    {
      ELLE_TRACE_SCOPE("%s: retrive progress", *this);

      float progress =
        surface::gap::operation_detail::progress::progress(this->etoile());

      ELLE_DEBUG("%s: progress is %s", *this, progress);

      reactor::Lock l(this->_progress_mutex);
      this->_progress = progress;
    }

    float
    TransferMachine::progress() const
    {
      reactor::Lock l(this->_progress_mutex);
      return this->_progress;
    };

    metrics::Metric
    TransferMachine::network_metric() const
    {
      return metrics::Metric{
        {MKey::value, this->network_id()},
      };
    }

    metrics::Metric
    TransferMachine::transaction_metric() const
    {
      auto const& transaction = *this->data();

      auto timestamp_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());
      auto timestamp_tr = std::chrono::duration<double>(transaction.mtime);
      double duration = timestamp_now.count() - timestamp_tr.count();

      bool peer_online = this->is_sender() ?
        this->_state.device_status(
          transaction.recipient_id, transaction.recipient_device_id):
        this->_state.device_status(
          transaction.sender_id, transaction.sender_device_id);

      std::string author{this->is_sender() ? "sender" : "recipient"};
      std::string sender_status{this->is_sender() ? "true"
                                                  : (peer_online ? "true"
                                                                 : "false")};

      std::string recipient_status{this->is_sender() ? "true"
                                                     : (peer_online ? "true"
                                                                    : "false")};

      return metrics::Metric{
        {MKey::author, author},
        {MKey::duration, std::to_string(duration)},
        {MKey::count, std::to_string(transaction.files_count)},
        {MKey::size, std::to_string(transaction.total_size)},
        {MKey::network, transaction.network_id},
        {MKey::value, transaction.id},
        {MKey::sender_online, sender_status},
        {MKey::recipient_online, recipient_status},
      };
    }

    /*----------.
    | Printable |
    `----------*/

    std::string
    TransferMachine::type() const
    {
      return "TransferMachine";
    }

    void
    TransferMachine::print(std::ostream& stream) const
    {
      auto const& data = *this->_data;
      auto const& me = this->state().me();

      stream << this->type() << "(id=" << this->id()
             << ", (u=" << me.id;
      if (!data.network_id.empty())
        stream << ", n=" << data.network_id;
      if (!data.id.empty())
        stream << ", t=" << data.id;
      stream << ")";
    }

    std::ostream&
    operator <<(std::ostream& out,
                TransferState const& t)
    {
      switch (t)
      {
        case TransferState_NewTransaction:
          return out << "NewTransaction";
        case TransferState_SenderCreateNetwork:
          return out << "SenderCreateNetwork";
        case TransferState_SenderCreateTransaction:
          return out << "SenderCreateTransaction";
        case TransferState_SenderCopyFiles:
          return out << "SenderCopyFiles";
        case TransferState_SenderWaitForDecision:
          return out << "SenderWaitForDecision";
        case TransferState_RecipientWaitForDecision:
          return out << "RecipientWaitForDecision";
        case TransferState_RecipientAccepted:
          return out << "RecipientAccepted";
        case TransferState_GrantPermissions:
          return out << "GrantPermissions";
        case TransferState_RecipientWaitForReady:
          return out << "RecipientWaitForReady";
        case TransferState_PublishInterfaces:
          return out << "PublishInterfaces";
        case TransferState_Connect:
          return out << "Connect";
        case TransferState_PeerDisconnected:
          return out << "PeerDisconnected";
        case TransferState_PeerConnectionLost:
          return out << "PeerConnectionLost";
        case TransferState_Transfer:
          return out << "Transfer";
        case TransferState_CleanLocal:
          return out << "CleanLocal";
        case TransferState_CleanRemote:
          return out << "CleanRemote";
        case TransferState_Finished:
          return out << "Finished";
        case TransferState_Rejected:
          return out << "Rejected";
        case TransferState_Canceled:
          return out << "Canceled";
        case TransferState_Failed:
          return out << "Failed";
      }
      return out;
    }

  }
}
