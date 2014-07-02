#ifndef SURFACE_GAP_ROUNDS_HH
# define SURFACE_GAP_ROUNDS_HH

# include <vector>
# include <string>

# include <infinit/oracles/meta/Client.hh> //XXX: Update fwd.hh.

# include <station/fwd.hh>

# include <reactor/fwd.hh>

# include <reactor/network/fwd.hh>

# include <elle/Printable.hh>
# include <elle/attribute.hh>

# include <memory>

namespace surface
{
  namespace gap
  {
    class Round:
      public elle::Printable
    {
    public:
      Round(std::string const& name);
      virtual
      ~Round();

      virtual
      std::unique_ptr<station::Host>
      connect(station::Station& station) = 0;

      ELLE_ATTRIBUTE_R(std::string, name);
    };

    class AddressRound:
      public Round
    {
    public:
      typedef std::vector<std::pair<std::string, int>> Endpoints;
      AddressRound(std::string const& name,
                   Endpoints enpoints);

      std::unique_ptr<station::Host>
      connect(station::Station& station) override;
      ELLE_ATTRIBUTE(Endpoints, endpoints);
    private:
      std::unique_ptr<station::Host>
      _connect(station::Station& station,
               std::pair<std::string, int> const& endpoint);

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };

    class FallbackRound:
      public Round
    {
    public:
      FallbackRound(std::string const& name,
                    infinit::oracles::meta::Client const& meta,
                    std::string const& uid);
      std::unique_ptr<station::Host>
      connect(station::Station& station) override;

    private:
      ELLE_ATTRIBUTE(infinit::oracles::meta::Client const&, meta);
      ELLE_ATTRIBUTE(std::string, uid);

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
