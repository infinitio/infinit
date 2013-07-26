#ifndef SURFACE_GAP_ROUNDS_HH
# define SURFACE_GAP_ROUNDS_HH

# include <vector>
# include <string>

# include <elle/attribute.hh>

namespace surface
{
    namespace gap
    {
      class Round
      {
      public:
        Round(std::string const& name);

        virtual
        ~Round();

        virtual
        std::vector<std::string>
        endpoints() = 0;

      private:
        ELLE_ATTRIBUTE_R(std::string, name);
      };

      class AddressRound:
        public Round
      {
      public:
        AddressRound(std::string const& name,
                     std::vector<std::string>&& enpoints);

        std::vector<std::string>
        endpoints() override;

        ELLE_ATTRIBUTE(std::vector<std::string>, endpoints);
      };

      class FallbackRound:
        public Round
      {
      public:
        FallbackRound(std::string const& name,
                      std::string const& host,
                      int port,
                      std::string const& uid);

        std::vector<std::string>
        endpoints() override;

      private:
        ELLE_ATTRIBUTE(std::vector<std::string>, endpoints);
        ELLE_ATTRIBUTE(std::string, host);
        ELLE_ATTRIBUTE(int, port);
        ELLE_ATTRIBUTE(std::string , uid);
      };
    }
}

#endif
