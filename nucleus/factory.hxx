#ifndef NUCLEUS_FACTORY_HXX
# define NUCLEUS_FACTORY_HXX

# include <elle/log.hh>

namespace nucleus
{
  namespace factory
  {
    /*----------.
    | Functions |
    `----------*/

    namespace setup
    {
      template <typename... A>
      static
      elle::utility::Factory<neutron::Component, A...>
      _block()
      {
        ELLE_LOG_COMPONENT("infinit.nucleus.factory");
        ELLE_DEBUG_FUNCTION("");

        elle::utility::Factory<neutron::Component, A...> factory;

        factory.template record<neutron::Object>(neutron::ComponentObject);
        // XXX[shouldn't be in neutron?]
        factory.template record<proton::Contents>(neutron::ComponentContents);
        factory.template record<neutron::Group>(neutron::ComponentGroup);

        return (factory);
      }
    }

    template <typename... A>
    elle::utility::Factory<neutron::Component, A...> const&
    block()
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.factory");
      ELLE_TRACE_FUNCTION("");

      static elle::utility::Factory<neutron::Component, A...> factory =
        setup::_block<A...>();

      return (factory);
    }

    template <typename... A>
    elle::utility::Factory<neutron::Component, A...> const&
    block(A&&... arguments)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.factory");
      ELLE_TRACE_FUNCTION("");

      return (block<A...>());
    }
  }
}

#endif
