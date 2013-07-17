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
      private:
        ELLE_ATTRIBUTE_RW(std::string, name);
      public:
        Round();
        Round(Round&& r);
        Round(Round const& r);
        virtual ~Round();

        virtual
        std::vector<std::string>
        endpoints() = 0;
      };

      class AddressRound:
        public Round
      {
      private:
        std::vector<std::string> _endpoints;
      public:
        std::vector<std::string>
        endpoints();

        void
        endpoints(std::vector<std::string> const&);
      };

      class FallbackRound:
        public Round
      {
      private:
        std::vector<std::string> _endpoints;
        ELLE_ATTRIBUTE(std::string, host);
        ELLE_ATTRIBUTE(int, port);
        ELLE_ATTRIBUTE(std::string , uid);
      public:
        FallbackRound(std::string const& host,
                      int port,
                      std::string const& uid);
        std::vector<std::string>
        endpoints();
      };
    }
}

#endif
