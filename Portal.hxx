#ifndef PORTAL_HXX
# define PORTAL_HXX

# include <elle/log.hh>

# include <cryptography/random.hh>

# include <reactor/Scope.hh>
# include <reactor/scheduler.hh>

# include <protocol/RPC.hh>
# include <protocol/Serializer.hh>

ELLE_LOG_COMPONENT("Portal");

namespace infinit
{
  /*-------------.
  | Construction |
  `-------------*/

  template <typename RPC>
  Portal<RPC>::Portal(std::string const& name,
                      std::function<void (RPC&)> initializer,
                      int port):
    _phrase(),
    _port(port),
    _initializer(initializer),
    _server(),
    _acceptor()
  {
    ELLE_TRACE_SCOPE("start portal");
    this->_server.reset(new reactor::network::TCPServer(
                          *reactor::Scheduler::scheduler()));
    this->_server->listen(this->_port);
    this->_port = this->_server->local_endpoint().port();
    ELLE_ASSERT_NEQ(this->_port, 0);
    ELLE_DEBUG("RPC listening on port %s", _port);
    this->_acceptor.reset(
      new reactor::Thread(*reactor::Scheduler::scheduler(),
                          elle::sprintf("RPC %s", *this),
                          [&] { this->_accept(); },
                          false));
    try
    {
      elle::String pass(cryptography::random::generate<elle::String>(4096));
      this->_phrase.Create(_port, pass);
      this->_phrase.store(Infinit::User, Infinit::Network, name);
    }
    catch (...)
    {
      this->_acceptor->terminate_now();
      throw;
    }
  }

  template <typename RPC>
  Portal<RPC>::~Portal()
  {
    this->_acceptor->terminate_now();
  }

  /*-------.
  | Server |
  `-------*/

  template <typename RPC>
  void
  Portal<RPC>::_accept()
  {
    int i = 0;
    reactor::Scope scope;

    while (true)
    {
      std::shared_ptr<reactor::network::TCPSocket> socket(
        this->_server->accept());

      ELLE_DEBUG("accepted new rpc control from %s", socket->remote_locus());

      i++;
      auto run = [&, socket]
        {
          ELLE_LOG_COMPONENT("infinit.hole.slug.Slug");
          try
          {
            infinit::protocol::Serializer serializer(
              *reactor::Scheduler::scheduler(), *socket);
            infinit::protocol::ChanneledStream channels(
              *reactor::Scheduler::scheduler(), serializer);
            RPC rpcs(channels);
            this->_initializer(rpcs);
            rpcs.parallel_run();
          }
          catch (elle::Exception const& e)
          {
            ELLE_WARN("slug control: %s", e.what());
          }
        };
      auto name = reactor::Scheduler::scheduler()->current()->name();
      scope.run_background(elle::sprintf("%s: pool %s", name, i), run);
      ELLE_TRACE("new connection accepted");
    }
  }
}

#endif
