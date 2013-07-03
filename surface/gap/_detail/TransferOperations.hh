#ifndef SURFACE_GAP_DETAILS_TRANSFEROPERATIONS_HH
# define SURFACE_GAP_DETAILS_TRANSFEROPERATIONS_HH

# include <etoile/fwd.hh>
# include <lune/fwd.hh>
# include <nucleus/fwd.hh>
# include <elle/types.hh>

# include <unordered_set>
# include <string>

namespace surface
{
  namespace gap
  {
    namespace operation_detail
    {
      namespace to
      {
        void
        send(etoile::Etoile& etoile,
             lune::Descriptor const& descriptor,
             nucleus::neutron::Subject const& subject,
             std::unordered_set<std::string> items);

        elle::Natural64
        send(etoile::Etoile& etoile,
             lune::Descriptor const& descriptor,
             nucleus::neutron::Subject const& subject,
             std::string const& source);
      }

      namespace from
      {
        void
        receive(etoile::Etoile& etoile,
                lune::Descriptor const& descriptor,
                nucleus::neutron::Subject const& subject,
                std::string const& target);
      }
    }
  }
}

#endif
