#ifndef PORTAL_HH
# define PORTAL_HH

# include <reactor/network/tcp-server.hh>
# include <reactor/thread.hh>

# include <lune/Phrase.hh>

namespace infinit
{
  template <typename RPC>
  class Portal
  {
  /*-------------.
  | Construction |
  `-------------*/
  public:
    Portal(std::string const& name,
           std::function<void (RPC&)> initializer,
           int port = 0);
    ~Portal();

  /*-------.
  | Portal |
  `-------*/
  private:
    ELLE_ATTRIBUTE(lune::Phrase, phrase);

  /*-------.
  | Server |
  `-------*/
  public:
    ELLE_ATTRIBUTE_R(int, port);
    ELLE_ATTRIBUTE(std::function<void (RPC&)>, initializer);
  private:
    std::unique_ptr<reactor::network::TCPServer> _server;
    std::unique_ptr<reactor::Thread> _acceptor;
    void _accept();
  };
}

# include <Portal.hxx>

#endif
