#ifndef SURFACE_GAP_ROUNDS_HH
# define SURFACE_GAP_ROUNDS_HH

# include <vector>
# include <string>

# include <station/fwd.hh>

# include <reactor/fwd.hh>

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

        /*----------.
        | Printable |
        `----------*/
        void
        print(std::ostream& stream) const override;

        virtual
        std::string const&
        type() const
        {
          static std::string const type{"Round"};
          return type;
        }

      private:
        ELLE_ATTRIBUTE_R(std::string, name);
      protected:
        std::vector<std::string> _endpoints;
      };

      class AddressRound:
        public Round
      {
      public:
        AddressRound(std::string const& name,
                     std::vector<std::string>&& enpoints);

        std::unique_ptr<station::Host>
        connect(station::Station& station) override;

        /*----------.
        | Printable |
        `----------*/
      public:
        std::string const&
        type() const override
        {
          static std::string const type{"AddressRound"};
          return type;
        }
      };

      class FallbackRound:
        public Round
      {
      public:
        FallbackRound(std::string const& name,
                      std::string const& host,
                      int port,
                      std::string const& uid);

        std::unique_ptr<station::Host>
        connect(station::Station& station) override;

      private:
        ELLE_ATTRIBUTE(std::string, host);
        ELLE_ATTRIBUTE(int, port);
        ELLE_ATTRIBUTE(std::string , uid);

        /*----------.
        | Printable |
        `----------*/
      public:
        std::string const&
        type() const override
        {
          static std::string const type{"FallbackRound"};
          return type;
        }
      };
    }
}

#endif
