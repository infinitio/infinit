#ifndef STATION_HOST_HH
# define STATION_HOST_HH

# include <reactor/network/tcp-socket.hh>

# include <papier/Passport.hh>

# include <station/fwd.hh>

namespace station
{
  /** Garanteed unique connection to a peer identified by its passpord,
    * and returned by a Station.
    * Underlying socket cannot survive the Host object or uniqueness semantic
    * is broken.
    */
  class Host:
    public elle::Printable
  {
  public:
    ~Host();
    reactor::network::Socket&
    socket();
    // Construct a host from an untracked socket. We do not track non p2p mode
    Host(std::unique_ptr<reactor::network::Socket>&& socket);
  private:
    friend class Station;
    Host(Station& owner,
         papier::Passport const& passport,
         std::unique_ptr<reactor::network::Socket>&& socket);

    ELLE_ATTRIBUTE(Station*, owner);
    ELLE_ATTRIBUTE_R(papier::Passport, passport);
    ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::Socket>, socket);

  public:

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
