#ifndef NUCLEUS_FACTORY_HH
# define NUCLEUS_FACTORY_HH

# include <elle/types.hh>
# include <elle/utility/Factory.hh>

# include <nucleus/proton/Nature.hh>

namespace nucleus
{
  namespace factory
  {

    /*----------.
    | Functions |
    `----------*/

    // XXX move this to Node.hh

    /// Return the factory capable of building tree node instances.
    elle::utility::Factory<proton::Nature> const&
    node();
  }
}

#endif
