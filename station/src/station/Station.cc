#include <elle/Exception.hh>
#include <elle/log.hh>
#include <elle/memory.hh>
#include <elle/finally.hh>

#include <reactor/scheduler.hh>

#include <station/AlreadyConnected.hh>
#include <station/InvalidPassport.hh>
#include <station/NetworkError.hh>
#include <station/Station.hh>

ELLE_LOG_COMPONENT("station.Station");

namespace station
{
  /*-------------.
  | Construction |
  `-------------*/

  Station::Station(papier::Authority const& authority,
                   papier::Passport const& passport):
    _authority(authority),
    _passport(passport),
    _server(*reactor::Scheduler::scheduler()),
    _server_thread(*reactor::Scheduler::scheduler(),
                   elle::sprintf("%s server thread", *this),
                   [this] { this->_serve(); })
  {
    _server.listen();
  }

  Station::~Station() noexcept(false)
  {
    while (!this->_host_new.empty())
      this->_host_new.pop();
    ELLE_ASSERT(this->_hosts.empty());
    this->_server_thread.terminate_now();
  }

  /*------.
  | Hosts |
  `------*/

  void
  Station::_host_remove(Host const& host)
  {
    ELLE_TRACE_SCOPE("%s: remove host %s", *this, host);
    ELLE_ASSERT_CONTAINS(this->_hosts, host.passport());
    this->_hosts.erase(host.passport());
  }

  /*-----------.
  | Connection |
  `-----------*/

  std::unique_ptr<Host>
  Station::connect(std::string const& host, int port)
  {
    ELLE_TRACE_SCOPE("%s: connect to %s:%s", *this, host, port);
    auto socket = elle::make_unique<reactor::network::TCPSocket>(
      *reactor::Scheduler::scheduler(), host, port);
    return this->_negotiate(std::move(socket));
  }

  /*-------.
  | Server |
  `-------*/

  int
  Station::port() const
  {
    return this->_server.port();
  }

  std::unique_ptr<Host>
  Station::accept()
  {
    reactor::Scheduler::scheduler()->current()->wait(this->_host_available);
    ELLE_ASSERT(!this->_host_new.empty());
    std::unique_ptr<Host> res = std::move(this->_host_new.front());
    this->_host_new.pop();
    if (this->_host_new.empty())
      this->_host_available.close();
    return res;
  }

  void
  Station::_serve()
  {
    while (true)
    {
      std::unique_ptr<reactor::network::TCPSocket> socket (
        this->_server.accept());
      ELLE_TRACE_SCOPE("%s: accept connection from %s", *this, socket->peer());
      try
      {
        auto host = _negotiate(std::move(socket));
        this->_hosts[host->passport()] = host.get();
        this->_host_new.push(std::move(host));
        this->_host_available.open();
      }
      catch (ConnectionFailure const& e)
      {
        ELLE_TRACE("%s: host was rejected: %s", *this, e.what());
      }
      catch (std::runtime_error const& e)
      {
        ELLE_ERR("%s: host was rejected because of unexpected exception: %s",
                 *this, e.what());
      }
    }
  }

  enum class NegotiationStatus
  {
    already_connected,
    invalid,
    succeeded,
  };

  std::unique_ptr<Host>
  Station::_negotiate(std::unique_ptr<reactor::network::TCPSocket> socket)
  {
    try
    {
      ELLE_TRACE_SCOPE("%s: negotiate connection with %s",
                       *this, socket->peer());
      elle::serialize::OutputBinaryArchive output(*socket);
      elle::serialize::InputBinaryArchive input(*socket);

      // Exchange passports.
      ELLE_DEBUG("%s: send pasport", *this)
      {
        output << this->passport();
        socket->flush();
      }
      papier::Passport remote;
      ELLE_DEBUG("%s: read remote passport", *this)
      {
        input >> remote;
      }
      ELLE_DEBUG("%s: peer authenticates with %s", *this, remote);
      ELLE_ASSERT_NEQ(remote, this->passport());

      // Check we are not already connected.
      auto hash = std::hash<papier::Passport>()(this->passport());
      auto remote_hash = std::hash<papier::Passport>()(remote);
      auto check_already = [&] ()
        {
          if (this->_hosts.find(remote) != this->_hosts.end())
          {
            ELLE_TRACE("%s: peer is already connected, reject", *this);
            output << NegotiationStatus::already_connected;
            socket->flush();
            throw AlreadyConnected();
          }
        };
      check_already();

      bool master = hash < remote_hash;

      elle::SafeFinally pop_negotiation;
      if (master)
      {
        // If we're already negotiating, wait.
        while (this->_host_negotiating.find(remote) != this->_host_negotiating.end())
        {
          reactor::Scheduler::scheduler()->current()->wait(
            this->_negotiation_ended);
          check_already();
        }
        this->_host_negotiating.insert(remote);
        pop_negotiation.action([&]
                               {
                                 this->_host_negotiating.erase(remote);
                                 this->_negotiation_ended.signal();
                               });
      }

      // Check peer passport.
      if (!remote.validate(this->authority()))
      {
        ELLE_TRACE("%s: peer has an invalid passport, reject", *this);
        output << NegotiationStatus::invalid;
        socket->flush();
        throw InvalidPassport();
      }

      // Accept peer.
      {
        ELLE_DEBUG("%s: accept peer", *this);
        output << NegotiationStatus::succeeded;
        socket->flush();
      }

      // Wait for peer response.
      NegotiationStatus status;
      input >> status;
      switch (status)
      {
        case NegotiationStatus::succeeded:
        {
          std::unique_ptr<Host> res(new Host(*this, remote, std::move(socket)));
          ELLE_LOG("%s: validate peer %s", *this, *res);
          ELLE_ASSERT_NCONTAINS(this->_hosts, remote);
          this->_hosts[remote] = res.get();
          return res;
        }
        case NegotiationStatus::already_connected:
        {
          ELLE_TRACE("%s: peer says we're already connected", *this);
          throw AlreadyConnected();
        }
        case NegotiationStatus::invalid:
        {
          ELLE_TRACE("%s: peer says our passport is invalid", *this);
          throw InvalidPassport();
        }
        default:
          throw ConnectionFailure(
            elle::sprintf("%s: peer yields invalid status: %s", *this, status));
      }
    }
    catch (reactor::network::Exception const& e)
    {
      // XXX: If flush fails, bad and fail bits are set and any subsequent call
      // to methods will silently fail and do nothing. Big up for std
      // streams. Mofo.
      socket->clear();
      throw NetworkError(e);
    }
    catch (...)
    {
      // Likewise.
      socket->clear();
    }
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Station::print(std::ostream& stream) const
  {
    stream << "Station(" << this << ")";
  }

  std::ostream&
  operator <<(std::ostream& output, Station const& station)
  {
    station.print(output);
    return output;
  }

}
