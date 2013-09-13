#include <surface/gap/TransferMachine.hh>
#include <surface/gap/Rounds.hh>
#include <surface/gap/State.hh>
#include <metrics/Metric.hh>

#include <common/common.hh>

#include <papier/Descriptor.hh>
#include <papier/Authority.hh>

# include <frete/Frete.hh>

# include <station/Station.hh>
# include <station/AlreadyConnected.hh>

#include <reactor/fsm/Machine.hh>
#include <reactor/exception.hh>
#include <reactor/Scope.hh>

#include <cryptography/oneway.hh>

#include <elle/container/list.hh>
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
    TransferMachine::Snapshot::Snapshot(Data const& data,
                                        State const state,
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
      _current_state(State::None),
      _state_changed("state changed"),
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
      _end_state(
        this->_machine.state_make(
          "end", std::bind(&TransferMachine::_end, this))),
      _finished("finished barrier"),
      _rejected("rejected barrier"),
      _canceled("canceled barrier"),
      _failed("failed barrier"),
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
      _core_paused_state(
        this->_core_machine.state_make(
          "core paused", std::bind(&TransferMachine::_core_paused, this))),
      _peer_online("peer online"),
      _peer_offline("peer offline"),
      _peer_connected("peer connected"),
      _peer_disconnected("peer connected"),
      _state(state),
      _data(std::move(data)),
      _progress(0.0f),
      _progress_mutex()
    {
      ELLE_TRACE_SCOPE("%s: creating transfer machine: %s", *this, this->_data);

      // Normal way.
      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_finish_state,
                                    reactor::Waitables{&this->_finished},
                                    true);

      this->_machine.transition_add(this->_finish_state,
                                    this->_end_state);

      // Cancel way.
      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(this->_cancel_state,
                                    this->_end_state);

      // Fail way.
      this->_machine.transition_add_catch(this->_transfer_core_state,
                                          this->_fail_state);

      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_fail_state,
                                    reactor::Waitables{&this->_failed}, true);

      this->_machine.transition_add(this->_fail_state,
                                    this->_end_state);
      // Reject.
      this->_machine.transition_add(this->_reject_state,
                                    this->_end_state);

      // The catch transitions just open the barrier to logging purpose.
      // The snapshot will be kept.
      this->_machine.transition_add_catch(this->_fail_state, this->_end_state)
        .action([this] { ELLE_ERR("%s: Failure failed", *this); });
      this->_machine.transition_add_catch(this->_cancel_state, this->_end_state)
        .action([this]
                {
                  ELLE_ERR("%s: Cancellation failed", *this);
                  this->_failed.open();
                });
      this->_machine.transition_add_catch(this->_finish_state, this->_end_state)
        .action([this]
                {
                  ELLE_ERR("%s: Termination failed", *this);
                  this->_failed.open();
                });

      /*-------------.
      | Core Machine |
      `-------------*/
      this->_core_machine.transition_add(
        this->_publish_interfaces_state,
        this->_connection_state,
        reactor::Waitables{&this->_peer_online});
      this->_core_machine.transition_add(
        this->_connection_state,
        this->_wait_for_peer_state,
        reactor::Waitables{&this->_peer_offline});
      this->_core_machine.transition_add(
        this->_wait_for_peer_state,
        this->_connection_state,
        reactor::Waitables{&this->_peer_online});
      this->_core_machine.transition_add(
        this->_connection_state,
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
        this->_connection_state,
        this->_transfer_state,
        reactor::Waitables{&this->_peer_connected});

      this->_core_machine.transition_add(
        this->_transfer_state,
        this->_core_stoped_state,
        reactor::Waitables{&this->_finished},
        true);

      this->_core_machine.transition_add_catch_specific<
        reactor::network::Exception>(
        this->_transfer_state,
        this->_connection_state)
        .action([this]
                {
                  ELLE_ERR("%s: peer disconnected", *this);
                  this->_host.reset();
                });

      this->_core_machine.transition_add_catch(
        this->_publish_interfaces_state,
        this->_core_stoped_state)
        .action([this]
                {
                  ELLE_ERR("%s: interface publication failed", *this);
                  this->_failed.open();
                });
      this->_core_machine.transition_add_catch(
        this->_wait_for_peer_state,
        this->_core_stoped_state)
        .action([this]
                {
                  ELLE_ERR("%s: peer wait failed", *this);
                  this->_failed.open();
                });

      this->_core_machine.transition_add_catch(
        this->_connection_state,
        this->_core_stoped_state)
        .action([this]
                {
                  ELLE_ERR("%s: connection failed", *this);
                  this->_failed.open();
                });

      this->_core_machine.transition_add_catch(
        this->_transfer_state,
        this->_core_stoped_state)
        .action([this]
                {
                  ELLE_ERR("%s: transfer failed", *this);
                  this->_failed.open();
                });
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
    TransferMachine::current_state(TransferMachine::State const& state)
    {
      ELLE_TRACE_SCOPE("%s: set new state to %s", *this, state);
      this->_current_state = state;
      this->_save_snapshot();
      this->_state_changed.signal();
    }

    TransferMachine::State
    TransferMachine::current_state() const
    {
      return this->_current_state;
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
            catch (reactor::Terminate const&)
            {
              ELLE_TRACE("%s: terminted", *this);
              throw;
            }
            catch (std::exception const&)
            {
              ELLE_ERR("%s: something went wrong while transfering", *this);
            }
          }});
      elle::Finally delete_core_machine(
        [this]
        {
          this->_core_machine_thread->terminate_now();
          this->_core_machine_thread.reset();
        });
      scheduler.current()->wait(*this->_core_machine_thread);

      if (this->_failed.opened())
        throw Exception(gap_error, "an error occured");

      ELLE_DEBUG("%s: transfer core finished", *this);
    }

    void
    TransferMachine::_end()
    {
      ELLE_TRACE_SCOPE("%s: finish transfer machine", *this);
      if (this->_failed.opened())
        ELLE_WARN("fail barrier was opened");
      if (this->_canceled.opened())
        ELLE_WARN("cancel barrier was opened");
    }

    void
    TransferMachine::_finish()
    {
      ELLE_TRACE_SCOPE("%s: machine finished", *this);
      this->current_state(State::Finished);
      this->_finalize(plasma::TransactionStatus::finished);
      ELLE_DEBUG("%s: finished", *this);
    }

    void
    TransferMachine::_reject()
    {
      ELLE_TRACE_SCOPE("%s: machine rejected", *this);
      this->current_state(State::Rejected);
      this->_finalize(plasma::TransactionStatus::rejected);
      ELLE_DEBUG("%s: rejected", *this);
    }

    void
    TransferMachine::_cancel()
    {
      ELLE_TRACE_SCOPE("%s: machine canceled", *this);
      this->current_state(State::Canceled);
      this->_finalize(plasma::TransactionStatus::canceled);
      ELLE_DEBUG("%s: canceled", *this);
    }

    void
    TransferMachine::_fail()
    {
      ELLE_TRACE_SCOPE("%s: machine failed", *this);
      this->current_state(State::Failed);
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
      ELLE_TRACE_SCOPE("%s: stop machine for transaction", *this);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

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

      this->_current_state = State::Over;
    }

    /*-------------.
    | Core Machine |
    `-------------*/
    void
    TransferMachine::_publish_interfaces()
    {
      ELLE_TRACE_SCOPE("%s: publish interfaces", *this);
      this->current_state(State::PublishInterfaces);

      auto& station = this->station();

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

      // XXX.

      this->state().meta().connect_device(
        this->data()->id,
        this->state().passport().id(),
        addresses,
        public_addresses);
      ELLE_DEBUG("%s: interfaces published", *this);
    }

    std::unique_ptr<station::Host>
    TransferMachine::_connect()
    {
      auto const& transaction = *this->data();

      auto endpoints = this->state().meta().device_endpoints(
        this->data()->id,
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
        addresses.emplace_back(new FallbackRound("fallback",
                                                 host,
                                                 port,
                                                 this->data()->id));
      }

      size_t tries = 0;
      for (auto const& r: addresses)
      {
        ++tries;
        ELLE_DEBUG("%s: round(%s): %s", *this, tries, r->name());

        std::unique_ptr<station::Host> host;
        reactor::Barrier host_found;

        elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
        {
          std::vector<std::string> endpoints;
          ELLE_DEBUG("%s: get endpoints for %s", *this, *r)
            r->endpoints();
          for (std::string const& endpoint: endpoints)
          {
            ELLE_DEBUG("%s: endpoint %s", *this, endpoint);
            auto fn = [this, endpoint, &host, &host_found]
              {
                try
                {
                  std::vector<std::string> result;
                  boost::split(result, endpoint, boost::is_any_of(":"));

                  auto const &ip = result[0];
                  auto const &port = result[1];
                  ELLE_DEBUG("connect(%s, %s)", ip, port)
                  // XXX: This statement is ok while we are connecting to one peer.
                  host = this->station().connect(ip, std::stoi(port));
                  host_found.open();
                  ELLE_LOG("%s: connection to %s succeed", *this, endpoint);
                }
                catch (station::AlreadyConnected const&)
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

            scope.run_background(elle::sprintf("connect_try(%s)", endpoint), fn);
          }

          scope.run_background("wait_accepted",
                               [&] ()
                               {
                                 this->station().host_available().wait();
                                 host = this->station().accept();
                                 host_found.open();
                               });

          host_found.wait(10_sec);
        };

        if (host)
          return host;

        ELLE_TRACE("%s: round %s failed", *this, tries);
      }

      throw Exception(gap_api_error, "no round succeed");
    }

    void
    TransferMachine::_connection()
    {
      ELLE_TRACE_SCOPE("%s: connecting peers", *this);
      this->current_state(State::Connect);

      this->_host = this->_connect();
      this->frete(); // Force lazy evaluation.
      this->_peer_connected.signal();
    }

    void
    TransferMachine::_wait_for_peer()
    {
      ELLE_TRACE_SCOPE("%s: waiting for peer to reconnect", *this);
      this->current_state(State::PeerDisconnected);
    }

    void
    TransferMachine::_transfer()
    {
      ELLE_TRACE_SCOPE("%s: start transfer operation", *this);
      this->current_state(State::Transfer);

      elle::Finally _2{[this] { this->_host.reset(); }};
      elle::Finally _3{[this] { this->_serializer.reset(); }};
      elle::Finally _4{[this] { this->_channels.reset(); }};
      elle::Finally _5{[this] { this->_frete.reset(); }};

      this->_transfer_operation();

      ELLE_TRACE_SCOPE("%s: end of transfer operation", *this);
    }

    void
    TransferMachine::_core_stoped()
    {
      ELLE_TRACE_SCOPE("%s: core machine stoped", *this);
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

    station::Station&
    TransferMachine::station()
    {
      ELLE_TRACE_SCOPE("%s: get station", *this);
      if (!this->_station)
      {
        ELLE_DEBUG_SCOPE("building station");
        this->_station.reset(
          new station::Station(papier::authority(), this->state().passport()));
      }
      ELLE_ASSERT(this->_station != nullptr);
      return *this->_station;
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
        {MKey::value, transaction.id},
        {MKey::sender_online, sender_status},
        {MKey::recipient_online, recipient_status},
      };
    }

    void
    TransferMachine::progress(float f)
    {
      reactor::Lock l(this->_progress_mutex);
      this->_progress = f;
    }

    float
    TransferMachine::progress() const
    {
      reactor::Lock l(this->_progress_mutex);
      return this->_progress;
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
      if (!data.id.empty())
        stream << ", t=" << data.id;
      stream << ")";
    }

    std::ostream&
    operator <<(std::ostream& out,
                TransferMachine::State const& t)
    {
      switch (t)
      {
        case TransferMachine::State::NewTransaction:
          return out << "NewTransaction";
        case TransferMachine::State::SenderCreateTransaction:
          return out << "SenderCreateTransaction";
        case TransferMachine::State::SenderWaitForDecision:
          return out << "SenderWaitForDecision";
        case TransferMachine::State::RecipientWaitForDecision:
          return out << "RecipientWaitForDecision";
        case TransferMachine::State::RecipientAccepted:
          return out << "RecipientAccepted";
        case TransferMachine::State::PublishInterfaces:
          return out << "PublishInterfaces";
        case TransferMachine::State::Connect:
          return out << "Connect";
        case TransferMachine::State::PeerDisconnected:
          return out << "PeerDisconnected";
        case TransferMachine::State::PeerConnectionLost:
          return out << "PeerConnectionLost";
        case TransferMachine::State::Transfer:
          return out << "Transfer";
        case TransferMachine::State::Finished:
          return out << "Finished";
        case TransferMachine::State::Rejected:
          return out << "Rejected";
        case TransferMachine::State::Canceled:
          return out << "Canceled";
        case TransferMachine::State::Failed:
          return out << "Failed";
        case TransferMachine::State::Over:
          return out << "Over";
        case TransferMachine::State::None:
          return out << "None";
      }
      return out;
    }

  }
}
