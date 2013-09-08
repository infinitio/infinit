#ifndef STATION_STATION_HH
# define STATION_STATION_HH

# include <queue>
# include <unordered_set>

# include <elle/attribute.hh>

# include <reactor/Barrier.hh>
# include <reactor/network/tcp-server.hh>
# include <reactor/signal.hh>
# include <reactor/thread.hh>

# include <papier/Authority.hh>
# include <papier/Passport.hh>

# include <station/Host.hh>

namespace station
{
  class Station
  {
  /*-------------.
  | Construction |
  `-------------*/
  public:
    /// Construct a Station.
    ///
    /// \param authority The authority that signed passports.
    /// \param passport  The passport for this host.
    Station(papier::Authority const& authority,
            papier::Passport const& passport);
    /// Destroy a Station.
    ///
    /// \throw TerminateException if terminated.
    ~Station() noexcept(false);

  /*------.
  | Hosts |
  `------*/
  private:
    friend class Host;
    typedef std::unordered_map<papier::Passport, Host*> Hosts;
    void
    _host_remove(Host const& host);
    ELLE_ATTRIBUTE(Hosts, hosts);
    ELLE_ATTRIBUTE(std::unordered_set<papier::Passport>, host_negotiating);

  /*---------------.
  | Authentication |
  `---------------*/
  private:
    /// The authority that signed passports.
    ELLE_ATTRIBUTE_R(papier::Authority, authority);
    /// The passport for this host.
    ELLE_ATTRIBUTE_R(papier::Passport, passport);

  /*-----------.
  | Connection |
  `-----------*/
  public:
    /// Connect to another station.
    ///
    /// \throw AlreadyConnected if we are already connected to this station.
    std::unique_ptr<Host>
    connect(std::string const& host, int port);

  /*-------.
  | Server |
  `-------*/
  public:
    int
    port() const;
    std::unique_ptr<Host>
    accept();
  private:
    /// Accept connections and negotiate.
    void
    _serve();
    ///
    std::unique_ptr<Host>
    _negotiate(std::unique_ptr<reactor::network::TCPSocket> socket);
    /// The TCP servers to receive connection.
    ELLE_ATTRIBUTE_R(reactor::network::TCPServer, server);
    /// The thread running this->_serve().
    ELLE_ATTRIBUTE  (reactor::Thread, server_thread);
    /// A barrier open iff a new host is available.
    ELLE_ATTRIBUTE_RX(reactor::Barrier, host_available);
    /// The new hosts.
    ELLE_ATTRIBUTE  (std::queue<std::unique_ptr<Host>>, host_new);
    /// Signals when a negotiation ended.
    ELLE_ATTRIBUTE  (reactor::Signal, negotiation_ended);
  /*----------.
  | Printable |
  `----------*/
  public:
    /// Print pretty representation to \a stream.
    virtual
    void
    print(std::ostream& stream) const;
  };

  std::ostream&
  operator <<(std::ostream& output, Station const& station);
}

#endif
