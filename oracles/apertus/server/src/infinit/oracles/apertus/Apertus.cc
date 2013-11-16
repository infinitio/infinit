#include <infinit/oracles/apertus/Apertus.hh>
#include <reactor/exception.hh>
#include <reactor/network/exception.hh>
#include <elle/Exception.hh>
#include <elle/log.hh>
#include <tuple>
#include <infinit/oracles/apertus/Accepter.hh>
#include <infinit/oracles/apertus/Transfer.hh>
#include <algorithm>

ELLE_LOG_COMPONENT("infinit.oracles.apertus.Apertus");

namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      Apertus::Apertus(std::string mhost, int mport,
                       std::string host, int port):
        Waitable("apertus"),
        _accepter(*reactor::Scheduler::scheduler(),
                  "apertus_accepter",
                  std::bind(&Apertus::_run, std::ref(*this))),
        _meta(mhost, mport),
        _uuid(boost::uuids::random_generator()()),
        _host(host),
        _port(port)
      {
        ELLE_LOG("Apertus");
      }

      Apertus::~Apertus()
      {
        ELLE_LOG("~Apertus");
        this->_accepter.terminate_now();

        for (auto const& client: this->_clients)
        {
          ELLE_TRACE("%s: kick pending users for transfer %s", *this, client.first);
          delete client.second;
          this->_clients.erase(client.first);
        }

        for (auto const& accepter: this->_accepters)
        {
          ELLE_TRACE("%s: kick running accepter %s", *this, accepter)
            this->_accepter_remove(*accepter);
        }

        while (!this->_workers.empty())
        {
          ELLE_TRACE_SCOPE("%s: kick running transfer", *this);

          auto const& worker = *this->_workers.begin();
          ELLE_DEBUG("%s", worker.first)
          {
            if (worker.second == nullptr)
              ELLE_WARN("???");
            else
            {
              ELLE_DEBUG("%s: to remove: %s", *this, worker.first)
                this->_transfer_remove(*worker.second);
            }
          }
        }

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
        serv.listen(_port);

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

      std::map<oracle::hermes::TID, reactor::network::TCPSocket*>&
      Apertus::get_clients()
      {
        return _clients;
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

        ELLE_DEBUG("%s: remove %s", *this, transfer.tid());
        Transfer* worker = this->_workers[transfer.tid()].release();
        ELLE_DEBUG("%s: released %s", *this, worker->tid());
        this->_workers.erase(transfer.tid());
        ELLE_DEBUG("%s: erased %s", *this, worker->tid());

        reactor::run_later(
          elle::sprintf("remove transfer %s", *worker),
          [worker]
          {
            ELLE_DEBUG("foo: %s", worker->tid());
            delete worker;
            ELLE_DEBUG("bar: %s", worker->tid());
          });
      }

      void
      Apertus::_accepter_remove(Accepter const& accepter)
      {
        reactor::run_later(
          elle::sprintf("remove accepter %s", accepter),
          [this, &accepter]
          {
            ELLE_WARN("deleted 0");

            // ELLE_ASSERT_CONTAINS(this->_accepters, &accepter);
            Accepters::iterator it =
              std::find_if(this->_accepters.begin(),
                           this->_accepters.end(),
                           [&accepter] (std::unique_ptr<Accepter> const& item)
                           {
                             return item.get() == &accepter;
                           });
            ELLE_ASSERT(it != this->_accepters.end());

            ELLE_DEBUG("foo");
            this->_accepters.erase(it);
            ELLE_DEBUG("bar");
          });
      }

    }
  }
}
