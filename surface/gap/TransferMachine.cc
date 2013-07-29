#include <surface/gap/TransferMachine.hh>
#include <surface/gap/Rounds.hh>
#include <surface/gap/State.hh>
#include <metrics/Metric.hh>

#include <common/common.hh>

#include <papier/Descriptor.hh>
#include <papier/Authority.hh>

#include <etoile/Etoile.hh>

#include <reactor/fsm/Machine.hh>
#include <reactor/exception.hh>

#include <elle/os/getenv.hh>
#include <elle/network/Interface.hh>
#include <elle/printf.hh>

#include <boost/filesystem.hpp>

#include <functional>

ELLE_LOG_COMPONENT("surface.gap.TransferMachine");

namespace surface
{
  namespace gap
  {
    //---------- TransferMachine -----------------------------------------------
    TransferMachine::TransferMachine(surface::gap::State const& state):
      _scheduler(),
      _scheduler_thread(),
      _machine(),
      _machine_thread(),
      _transfer_core_state(
        this->_machine.state_make(
          "transfer core", std::bind(&TransferMachine::_transfer_core, this))),
      _finish_state(
        this->_machine.state_make(
          "finish", std::bind(&TransferMachine::_finish, this))),
      _cancel_state(
        this->_machine.state_make(
          "cancel", std::bind(&TransferMachine::_cancel, this))),
      _fail_state(
        this->_machine.state_make(
          "fail", std::bind(&TransferMachine::_fail, this))),
      _clean_state(
        this->_machine.state_make(
          "clean", std::bind(&TransferMachine::_clean, this))),
      _core_machine(),
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
      _state(state)
    {
      ELLE_TRACE_SCOPE("%s: creating transfer machine", *this);

      this->_machine.transition_add(_transfer_core_state,
                                    _finish_state,
                                    reactor::Waitables{&this->_finished});

      this->_machine.transition_add(_transfer_core_state,
                                    _cancel_state,
                                    reactor::Waitables{&this->_canceled});

      this->_machine.transition_add_catch(_transfer_core_state,
                                          _fail_state);

      this->_machine.transition_add(_finish_state,
                                    _clean_state);

      this->_machine.transition_add(_cancel_state,
                                    _clean_state);

      this->_machine.transition_add(_fail_state,
                                    _clean_state);

      this->_core_machine.transition_add(_publish_interfaces_state,
                                         _connection_state,
                                         reactor::Waitables{&this->_peer_online});
      this->_core_machine.transition_add(_connection_state,
                                         _wait_for_peer_state,
                                         reactor::Waitables{&this->_peer_offline});
      this->_core_machine.transition_add(_wait_for_peer_state,
                                         _connection_state,
                                         reactor::Waitables{&this->_peer_online});
      this->_core_machine.transition_add(_connection_state,
                                         _connection_state,
                                         reactor::Waitables{&this->_peer_online});
      this->_core_machine.transition_add(_connection_state,
                                         _transfer_state);

      // Cancel.
      this->_core_machine.transition_add(_publish_interfaces_state, _core_stoped_state, reactor::Waitables{&this->_canceled}, true);
      this->_core_machine.transition_add(_connection_state, _core_stoped_state, reactor::Waitables{&this->_canceled}, true);
      this->_core_machine.transition_add(_transfer_core_state, _core_stoped_state, reactor::Waitables{&this->_canceled}, true);

      // Exception.
      this->_core_machine.transition_add_catch(_publish_interfaces_state, _core_stoped_state);
      this->_core_machine.transition_add_catch(_connection_state, _core_stoped_state);
      this->_core_machine.transition_add_catch(_transfer_core_state, _core_stoped_state);
    }

    TransferMachine::~TransferMachine()
    {
      ELLE_TRACE_SCOPE("%s: destroying transfer machine", *this);
    }

    void
    TransferMachine::_transfer_core()
    {
      ELLE_TRACE_SCOPE("%s: start transfer core machine", *this);
      this->_core_machine.run();
    }

    void
    TransferMachine::_clean()
    {
      ELLE_TRACE_SCOPE("%s: clean %s", *this, this->transaction_id());
      auto path = common::infinit::network_directory(
        this->state().me().id, this->network_id());

      if (boost::filesystem::exists(path))
      {
        ELLE_DEBUG("%s: remove network at %s", *this, path);

        boost::filesystem::remove_all(path);
      }

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

      ELLE_DEBUG("finalize etoile")
        this->_etoile.reset();
      ELLE_DEBUG("finalize hole")
        this->_hole.reset();

      ELLE_DEBUG("%s: cleaned", *this);
    }

    void
    TransferMachine::_finish()
    {
      ELLE_TRACE_SCOPE("%s: machine finished", *this);
      this->_finiliaze(plasma::TransactionStatus::finished);
      ELLE_DEBUG("%s: finished", *this);
    }

    void
    TransferMachine::_cancel()
    {
      ELLE_TRACE_SCOPE("%s: machine canceled", *this);
      this->_finiliaze(plasma::TransactionStatus::canceled);
      ELLE_DEBUG("%s: canceled", *this);
    }

    void
    TransferMachine::_fail()
    {
      ELLE_TRACE_SCOPE("%s: machine failed", *this);
      this->_finiliaze(plasma::TransactionStatus::failed);
      ELLE_DEBUG("%s: failed", *this);
    }

    void
    TransferMachine::_finiliaze(plasma::TransactionStatus status)
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

      if (!this->_transaction_id.empty())
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

      if (!this->_network_id.empty())
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
    TransferMachine::run(reactor::fsm::State& initial_state)
    {
      ELLE_TRACE_SCOPE("%s: running transfer machine", *this);
      ELLE_ASSERT(this->_scheduler_thread == nullptr);

      this->_machine_thread.reset(
        new reactor::Thread{
          this->_scheduler,
          "run",
          [&] { this->_machine.run(initial_state); }});

      this->_scheduler_thread.reset(
        new std::thread{
          [&]
          {
            try
            {
              this->_scheduler.run();
            }
            catch (...)
            {
              ELLE_ERR("scheduling of network(%s) failed. Storing exception: %s",
                       this->_network_id, elle::exception_string());
              // this->exception = std::current_exception();
            }
          }
        });
    }

    void
    TransferMachine::cancel()
    {
      ELLE_TRACE_SCOPE("%s: cancel transaction %s", *this, this->transaction_id());
      this->_canceled.open();
    }

    bool
    TransferMachine::concerns_network(std::string const& network_id)
    {
      return this->_network_id == network_id;
    }

    bool
    TransferMachine::concerns_transaction(std::string const& transaction_id)
    {
      return this->_transaction_id == transaction_id;
    }

    bool
    TransferMachine::concerns_user(std::string const& user_id)
    {
      return (user_id == this->state().me().id) || (user_id == this->_peer_id);
    }

    void
    TransferMachine::_stop()
    {
      ELLE_TRACE_SCOPE("%s: stop machine for transaction %s",
                       *this, this->_network_id);

      ELLE_ASSERT(this->_scheduler_thread != nullptr);

      this->_scheduler_thread->join();
      this->_scheduler_thread.reset();
    }

    /*-------------.
    | Core Machine |
    `-------------*/
    void
    TransferMachine::_publish_interfaces()
    {
      ELLE_TRACE_SCOPE("%s: publish interfaces", *this);
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

      auto const& transaction = this->state().transaction_manager().one(
        this->transaction_id());

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
          std::unique_ptr<reactor::Thread> thread_ptr{
            new reactor::Thread (this->_scheduler,
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
          auto _this_thread = this->_scheduler.current();
          ELLE_DEBUG("waiting for new host");
          succeed = _this_thread->wait(this->hole().new_connected_host(), 10_sec);
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
    }

    void
    TransferMachine::_transfer()
    {
      ELLE_TRACE_SCOPE("%s: start transfer operation", *this);
      this->_transfer_operation();
      ELLE_TRACE_SCOPE("%s: end of transfer operation", *this);
    }

    void
    TransferMachine::_core_stoped()
    {
      ELLE_TRACE_SCOPE("%s: core machine stoped", *this);
    }

    /*-----------.
    | Attributes |
    `-----------*/
    std::string const&
    TransferMachine::transaction_id() const
    {
      ELLE_ASSERT_GT(this->_transaction_id.length(), 0u);
      return this->_transaction_id;
    }

    void
    TransferMachine::transaction_id(std::string const& id)
    {
      this->_transaction_id = id;
    }

    std::string const&
    TransferMachine::network_id() const
    {
      ELLE_ASSERT_GT(this->_network_id.length(), 0u);
      return this->_network_id;
    }

    void
    TransferMachine::network_id(std::string const& id)
    {
      this->_network_id = id;
    }

    std::string const&
    TransferMachine::peer_id() const
    {
      ELLE_ASSERT_GT(this->_peer_id.length(), 0u);
      return this->_peer_id;
    }

    void
    TransferMachine::peer_id(std::string const& id)
    {
      this->_peer_id = id;
    }

    bool
    TransferMachine::is_sender()
    {
      return this->_state.me().id ==
        this->_state.transaction_manager().one(this->transaction_id()).sender_id;
    }

    nucleus::proton::Network&
    TransferMachine::network()
    {
      if (!this->_network)
      {
        this->_network.reset(
          new nucleus::proton::Network(this->network_id()));
      }

      ELLE_ASSERT(this->_network != nullptr);
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
          this->state().network_manager().one(this->network_id()).descriptor;

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

    metrics::Metric
    TransferMachine::network_metric()
    {
      return metrics::Metric{
        {MKey::value, this->network_id()},
      };
    }

    metrics::Metric
    TransferMachine::transaction_metric()
    {
      auto const& transaction =
        this->state().transaction_manager().one(this->transaction_id());

      auto timestamp_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());
      auto timestamp_tr = std::chrono::duration<double>(transaction.timestamp);
      double duration = timestamp_now.count() - timestamp_tr.count();

      bool peer_online = this->is_sender() ?
        this->state().user_manager().device_status(
          transaction.recipient_id, transaction.recipient_device_id):
        this->state().user_manager().device_status(
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
      stream << this->type() << "(u=" << this->state().me().id;
      if (!this->_network_id.empty())
        stream << ", n=" << this->_network_id;
      if (!this->_transaction_id.empty())
        stream << ", t=" << this->_transaction_id;
      stream << ")";
    }
  }
}
