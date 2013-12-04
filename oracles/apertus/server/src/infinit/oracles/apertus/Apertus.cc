#include <infinit/oracles/apertus/Apertus.hh>

#include <infinit/oracles/apertus/Accepter.hh>
#include <infinit/oracles/apertus/Transfer.hh>

#include <reactor/exception.hh>
#include <reactor/network/exception.hh>
#include <reactor/http/exceptions.hh>

#include <elle/Exception.hh>
#include <elle/HttpClient.hh> // XXX: Remove that. Only for exception.
#include <elle/log.hh>

#include <boost/uuid/uuid_io.hpp>

#include <algorithm>
#include <tuple>

ELLE_LOG_COMPONENT("infinit.oracles.apertus.Apertus");
namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      Apertus::Apertus(std::string mhost, int mport,
                       std::string host, int port,
                       boost::posix_time::time_duration const& tick_rate):
        Waitable("apertus"),
        _accepter(*reactor::Scheduler::scheduler(),
                  "apertus_accepter",
                  std::bind(&Apertus::_run, std::ref(*this))),
        _meta(mhost, mport),
        _uuid(boost::uuids::random_generator()()),
        _host(host),
        _port(port),
        _bandwidth(0),
        _tick_rate(tick_rate),
        _monitor(*reactor::Scheduler::scheduler(),
                 "apertus_monitor",
                 std::bind(&Apertus::_run_monitor, std::ref(*this)))
      {
        ELLE_LOG("Apertus");
      }

      Apertus::~Apertus()
      {
        ELLE_LOG("~Apertus");
        this->_monitor.terminate_now();
        this->_accepter.terminate_now();

        for (auto const& client: this->_clients)
        {
          ELLE_TRACE("%s: kick pending users for transfer %s", *this, client.first);
          delete client.second;
          this->_clients.erase(client.first);
        }

        while (!this->_accepters.empty())
        {
          ELLE_DEBUG("%s: remaining accepters: %s", *this, this->_accepters.size());
          Accepter* accepter = *this->_accepters.begin();
          ELLE_TRACE("%s: kick running accepter %s", *this, accepter)
            this->_accepter_remove(*accepter);
        }

        while (!this->_workers.empty())
        {
          ELLE_TRACE_SCOPE("%s: kick running transfer", *this);

          auto const& worker = *this->_workers.begin();
          ELLE_DEBUG("%s: transfer %s", *this, worker.first)
          {
            ELLE_ASSERT(worker.second != nullptr);
            ELLE_DEBUG("%s: to remove: %s", *this, worker.first)
              this->_transfer_remove(*worker.second);
          }
        }

        ELLE_ASSERT(this->_workers.empty());
        ELLE_ASSERT(this->_accepters.empty());
        ELLE_ASSERT(this->_clients.empty());
      }

      void
      Apertus::_register()
      {
        this->_meta.register_apertus(this->_uuid, this->_port);
      }

      void
      Apertus::_unregister()
      {
        this->_meta.unregister_apertus(this->_uuid);
      }

      void
      Apertus::_run()
      {
        reactor::network::TCPServer serv(*reactor::Scheduler::scheduler());
        serv.listen(this->_port);
        this->_port = serv.port();

        this->_register();

        elle::With<elle::Finally>([this] { this->_unregister(); }) << [&]
        {
          std::unique_ptr<reactor::network::TCPSocket> client{nullptr};
          while (true)
          {
            ELLE_TRACE("%s: waiting for new client", *this);
            ELLE_ASSERT(client == nullptr);
            client.reset(serv.accept());

            this->_accepters.emplace(new Accepter(*this, std::move(client)));
          }
        };
      }

      void
      Apertus::stop()
      {
        this->_signal();
      }

      void
      Apertus::_connect(oracle::hermes::TID tid,
                        std::unique_ptr<reactor::network::TCPSocket> client1,
                        std::unique_ptr<reactor::network::TCPSocket> client2)
      {
        if (this->_workers.find(tid) != this->_workers.end())
          this->_workers.erase(tid);

        ELLE_ASSERT(client1 != nullptr);
        ELLE_ASSERT(client2 != nullptr);

        this->_workers[tid] =
          std::move(std::unique_ptr<Transfer>(
            new Transfer(*this, tid, std::move(client1), std::move(client2))));
      }

      void
      Apertus::_transfer_remove(Transfer const& transfer)
      {
        // ELLE_ASSERT_CONTAINS(this->_workers, transfer.tid());
        ELLE_ASSERT(this->_workers[transfer.tid()] != nullptr);

        ELLE_DEBUG("remove %s", transfer.tid());
        Transfer* worker = this->_workers[transfer.tid()].release();
        ELLE_DEBUG("released %s", worker->tid());
        this->_workers.erase(transfer.tid());
        ELLE_DEBUG("erased %s", worker->tid());

        reactor::run_later(
          elle::sprintf("delete transfer %s", *worker),
          [worker]
          {
            ELLE_DEBUG("about to delete transfer: %s", *worker);
            delete worker;
          });
      }

      void
      Apertus::_accepter_remove(Accepter const& accepter)
      {
        ELLE_TRACE_SCOPE("%s: remove accepter %s", *this, accepter);
        Accepter* accepter_ptr = const_cast<Accepter*>(&accepter);

        ELLE_DEBUG("accepter address: %s", accepter_ptr);
        // ELLE_ASSERT(this->_accepters
        // ELLE_ASSERT_CONTAINS(this->_accepters, &accepter);
        ELLE_ASSERT(accepter_ptr != nullptr);

        ELLE_DEBUG("erase accepter");
        size_t removed = this->_accepters.erase(accepter_ptr);
        ELLE_ASSERT_EQ(removed, 1u);

        ELLE_DEBUG("run later: delete accepter %s", accepter_ptr);
        reactor::run_later(
          elle::sprintf("delete accepter %s", *accepter_ptr),
          [accepter_ptr]
          {
            ELLE_DEBUG("about to delete accepter: %s", *accepter_ptr);
            delete accepter_ptr;
            //accepter_ptr = nullptr;
          });
      }

      /*----------.
      | Printable |
      `----------*/
      void
      Apertus::print(std::ostream& stream) const
      {
        stream << "Apertus(" << this->_uuid << ")";
      }

      /*-----------.
      | Monitoring |
      `-----------*/
      void
      Apertus::add_to_bandwidth(uint32_t data)
      {
        _bandwidth += data;
      }

      void
      Apertus::_run_monitor()
      {
        while (true)
        {
          reactor::sleep(_tick_rate);

          uint32_t bdwps = _bandwidth / _tick_rate.total_seconds();
          ELLE_TRACE("%s: bandwidth is currently estimated at %sB/s",
            *this, bdwps);

          try
          {
            this->_meta.apertus_update_bandwidth(_uuid, bdwps, _workers.size());
          }
          catch (reactor::http::RequestError const&)
          {
            ELLE_WARN("%s: unable to update bandwidth on meta: %s",
                      *this, elle::exception_string());
          }
          catch (elle::http::Exception const&)
          {
            ELLE_WARN("%s: unable to update bandwidth on meta: %s",
                      *this, elle::exception_string());
          }

          _bandwidth = 0;
        }
      }
    }
  }
}
