#ifndef FIST_SURFACE_GAP_TESTS_TROPHONIUS_HH
# define FIST_SURFACE_GAP_TESTS_TROPHONIUS_HH

# include <memory>
# include <unordered_map>
# include <vector>

# include <elle/UUID.hh>
# include <elle/attribute.hh>

# include <reactor/network/ssl-server.hh>
# include <reactor/thread.hh>

# include <elle/UUID.hh>

namespace tests
{
  class Trophonius
  {
  public:
    Trophonius();
    ~Trophonius();
    int
    port() const;

  protected:
    virtual
    void
    _serve();
    virtual
    void
    _serve(std::unique_ptr<reactor::network::SSLSocket> socket);

  public:
    std::string
    json() const;

    void
    disconnect_all_users();

  private:
    ELLE_ATTRIBUTE(reactor::network::SSLServer, server);
    ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter);
    typedef std::unordered_map<std::string, std::unique_ptr<reactor::network::SSLSocket>> Clients;
    ELLE_ATTRIBUTE(Clients, clients);

  public:
    std::vector<reactor::network::SSLSocket*>
    clients(elle::UUID const& user);

    reactor::network::SSLSocket*
    socket(elle::UUID const& user,
           elle::UUID const& device);

  };
}

#endif
