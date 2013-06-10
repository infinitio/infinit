#ifndef NUCLEUS_FACTORY_HH
# define NUCLEUS_FACTORY_HH

# include <elle/types.hh>
# include <elle/utility/Factory.hh>

# include <nucleus/proton/Nature.hh>
# include <nucleus/proton/Contents.hh>
# include <nucleus/proton/Seam.hh>
# include <nucleus/proton/Quill.hh>
# include <nucleus/neutron/Object.hh>
# include <nucleus/neutron/Data.hh>
# include <nucleus/neutron/Catalog.hh>
# include <nucleus/neutron/Reference.hh>
# include <nucleus/neutron/Access.hh>
# include <nucleus/neutron/Group.hh>
# include <nucleus/neutron/Ensemble.hh>
# include <nucleus/neutron/Attributes.hh>

namespace nucleus
{
  namespace factory
  {

    /*----------.
    | Functions |
    `----------*/

    /// Return the factory capable of building nucleus block instances
    /// by calling the constructor matching the given type sequence.
    template <typename... A>
    elle::utility::Factory<neutron::Component, A...> const&
    block();
    /// Return the factory capable of building nucleus block instances
    /// for the given arguments
    template <typename... A>
    elle::utility::Factory<neutron::Component, A...> const&
    block(A&&... arguments);

    // XXX
    /// Return the factory capable of building tree node instances.
    elle::utility::Factory<proton::Nature> const&
    node();
  }
}

# include <nucleus/factory.hxx>

#endif
