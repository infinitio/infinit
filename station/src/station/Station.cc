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
                   papier::Passport const& passport,
                   std::string const& name):
    _authority(authority),
    _passport(passport),
    _name(name),
    _server(true),
    _server_thread(*reactor::Scheduler::scheduler(),
                   elle::sprintf("%s server thread", *this),
                   [this] { this->_serve(); })
  {
    this->_server.listen();
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
    auto socket = elle::make_unique<reactor::network::TCPSocket>(host, port);
    std::unique_ptr<Host> res = this->_negotiate(std::move(socket));
    ELLE_TRACE("%s: connect succeeded with %s", *this, host);
    return res;
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
      std::unique_ptr<reactor::network::Socket> socket;
      try
      {
        socket = this->_server.accept();
      }
      // https://app.asana.com/0/5058254180090/14728698282583
      catch (reactor::Terminate const&)
      {
        throw;
      }
      catch (...)
      {
        ELLE_ERR("%s: fatal error accepting host: %s",
                 *this, elle::exception_string());
        throw;
      }
      ELLE_TRACE_SCOPE("%s: accept connection from %s", *this, socket->peer());
      try
      {
        auto host = _negotiate(std::move(socket));
        ELLE_DEBUG("%s: accept negotiation with %s", *this, *host);
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
      // https://app.asana.com/0/5058254180090/14728698282583
      catch (reactor::Terminate const&)
      {
        throw;
      }
      catch (...)
      {
        ELLE_ERR("%s: fatal error negotiating host: %s",
                 *this, elle::exception_string());
        throw;
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
  Station::_negotiate(std::unique_ptr<reactor::network::Socket> socket)
  {
    ELLE_TRACE_SCOPE("%s: negotiate connection with %s",
                     *this, socket->peer());
    // Exchange protocol version.
    char version = 0;
    socket->write(elle::ConstWeakBuffer(&version, 1));
    auto remote_protocol = socket->read(1)[0];
    (void)remote_protocol;
    try
    {
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
      bool master = this->passport() < remote;

      ELLE_DEBUG("%s: assume %s role", *this, master ? "master" : "slave")
      {
        ELLE_DUMP("%s: local passport: %s", *this, this->passport());
        ELLE_DUMP("%s: remote passport: %s", *this, remote);
      }

      elle::SafeFinally pop_negotiation;
      if (master)
      {
        // If we're already negotiating, wait.
        auto& negotiating = this->_host_negotiating;
        while (negotiating.find(remote) != negotiating.end())
        {
          ELLE_DEBUG("%s: already negotiating with this peer, wait", *this);
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
          ELLE_LOG("%s: validate peer %s", *this, remote);
          if (master)
            ELLE_ASSERT_NCONTAINS(this->_hosts, remote);
          else if (this->_hosts.find(remote) != this->_hosts.end())
          {
            // FIXME: add sequence ids as soon as the station protocol version
            // is fixed (0.9.7).
            ELLE_ERR("%s: station conflict, need sequence ids to solve", *this);
            throw ConnectionFailure(
              elle::sprintf("%s: conflict on %s", *this, remote));
          }
          std::unique_ptr<Host> res(new Host(*this, remote, std::move(socket)));
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
      throw;
    }
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Station::print(std::ostream& stream) const
  {
    if (this->_name.empty())
      stream << "Station(" << this << ")";
    else
      stream << this->_name;
  }

  std::ostream&
  operator <<(std::ostream& output, Station const& station)
  {
    station.print(output);
    return output;
  }

}
