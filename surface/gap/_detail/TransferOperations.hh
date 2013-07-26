#ifndef SURFACE_GAP_DETAILS_TRANSFEROPERATIONS_HH
# define SURFACE_GAP_DETAILS_TRANSFEROPERATIONS_HH

# include <unordered_set>
# include <string>

# include <elle/types.hh>

# include <papier/fwd.hh>
# include <etoile/fwd.hh>
# include <lune/fwd.hh>

# include <nucleus/fwd.hh>

// XXX
# include <nucleus/neutron/Group.hh>

namespace surface
{
  namespace gap
  {
    namespace operation_detail
    {
      namespace blocks
      {
        struct NetworkBlocks
        {
          elle::io::Unique root_block;
          elle::io::Unique root_address;
          elle::io::Unique group_block;
          elle::io::Unique group_address;
        };

        NetworkBlocks
        create(std::string const& id,
               papier::Identity const& identity);
      }

      namespace user
      {
        void
        add(etoile::Etoile& etoile,
            nucleus::neutron::Group::Identity const& group,
            nucleus::neutron::Subject const& subject);

        void
        set_permissions(etoile::Etoile& etoile,
                        nucleus::neutron::Subject const& subject,
                        nucleus::neutron::Permissions const& permission);
      }

      namespace to
      {
        void
        send(etoile::Etoile& etoile,
             papier::Descriptor const& descriptor,
             nucleus::neutron::Subject const& subject,
             std::unordered_set<std::string> items);
      }

      namespace from
      {
        void
        receive(etoile::Etoile& etoile,
                papier::Descriptor const& descriptor,
                nucleus::neutron::Subject const& subject,
                std::string const& target);
      }

      namespace progress
      {
        float
        progress(etoile::Etoile& etoile);
      }
    }
  }
}

#endif
