#ifndef SURFACE_GAP_ROUNDS_HH
# define SURFACE_GAP_ROUNDS_HH

# include <vector>
# include <string>

# include <elle/attribute.hh>

namespace surface
{
    namespace gap
    {
      class round
      {
      private:
        ELLE_ATTRIBUTE_RW(std::vector<std::string>, endpoints);
        ELLE_ATTRIBUTE_RW(std::string, name);
      public:
        round();
        round(round&& r);
        round(round const& r);
      };
    }
}

#endif
