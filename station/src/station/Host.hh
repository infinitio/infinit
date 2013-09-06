#ifndef STATION_HOST_HH
# define STATION_HOST_HH

# include <reactor/network/tcp-socket.hh>

# include <papier/Passport.hh>

# include <station/fwd.hh>

namespace station
{
  class Host:
    public elle::Printable
  {
  public:
    ~Host();
    reactor::network::TCPSocket&
    socket();

  private:
    friend class Station;
    Host(Station& owner,
         papier::Passport const& passport,
         std::unique_ptr<reactor::network::TCPSocket>&& socket);
    ELLE_ATTRIBUTE(Station&, owner);
    ELLE_ATTRIBUTE_R(papier::Passport, passport);
    ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::TCPSocket>, socket);

    /*----------.
    | Printable |
    `----------*/
  public:
    virtual
    void
    print(std::ostream& stream) const override;
  };
}

#endif
