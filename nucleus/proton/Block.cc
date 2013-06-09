#include <cryptography/PublicKey.hh>
#include <cryptography/random.hh>

#include <nucleus/proton/Block.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Contents.hh>
#include <nucleus/proton/Seam.hh>
#include <nucleus/proton/Quill.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Data.hh>
#include <nucleus/neutron/Catalog.hh>
#include <nucleus/neutron/Reference.hh>
#include <nucleus/neutron/Access.hh>
#include <nucleus/neutron/Group.hh>
#include <nucleus/neutron/Ensemble.hh>
#include <nucleus/neutron/Attributes.hh>
#include <nucleus/Exception.hh>

#include <elle/utility/Factory.hh>
#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.nucleus.proton.Block");

namespace nucleus
{
  namespace proton
  {
    /*----------.
    | Constants |
    `----------*/

    cryptography::oneway::Algorithm const Block::Constants::oneway_algorithm(
      cryptography::oneway::Algorithm::sha256);

    /*-------------.
    | Construction |
    `-------------*/

    Block::Block():
      _component(neutron::ComponentUnknown),
      _state(State::clean)
    {
    }

    Block::Block(Network const network,
                 Family const family,
                 neutron::Component const component,
                 cryptography::PublicKey const& creator_K):
      _network(network),
      _family(family),
      _component(component),
      _creator(cryptography::oneway::hash(creator_K,
                                          Block::Constants::oneway_algorithm)),
      _creation_timestamp(elle::utility::Time::current()),
      _salt(cryptography::random::generate<elle::Natural64>()),
      _state(State::clean)
    {
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(Block)
    {
    }

    Block::~Block()
    {
    }

    /*-----------.
    | Interfaces |
    `-----------*/

    elle::Status
    Block::Dump(const elle::Natural32       margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[Block]" << std::endl;

      if (this->_network.Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the network");

      std::cout << alignment << elle::io::Dumpable::Shift << "[Family] "
                << std::dec << this->_family << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift << "[Component] "
                << std::dec << this->_component << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift << "[Creator K] "
                << this->_creator << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Creation Timestamp]" << std::endl;

      if (this->_creation_timestamp.Dump(margin + 4) == elle::Status::Error)
        throw Exception("unable to dump the timestamp");

      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Salt] " << this->_salt << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift << "[State] "
                << std::dec << this->_state << std::endl;

      return elle::Status::Ok;
    }

    void
    Block::print(std::ostream& stream) const
    {
      stream << "block{"
             << this->_network
             << ", "
             << this->_family
             << ", "
             << this->_component
             << ", "
             << this->_state
             << "}";
    }

    namespace block
    {
      /*----------.
      | Functions |
      `----------*/

      template <typename... A>
      static
      void
      _setup(elle::utility::Factory<neutron::Component, A...>& factory)
      {
        ELLE_DEBUG_FUNCTION(factory);

        factory.template record<neutron::Object>(neutron::ComponentObject);
        // XXX[shouldn't be in neutron?]
        factory.template record<proton::Contents>(neutron::ComponentContents);
        factory.template record<neutron::Group>(neutron::ComponentGroup);
      }

      template <typename... A>
      elle::utility::Factory<neutron::Component, A...> const&
      factory()
      {
        ELLE_TRACE_FUNCTION("");

        static elle::utility::Factory<neutron::Component, A...> factory;

        _setup<A...>(factory);

        return (factory);
      }

      template <typename... A>
      elle::utility::Factory<neutron::Component, A...> const&
      factory(A&&... arguments)
      {
        ELLE_TRACE_FUNCTION("");

        return (factory<A...>());
      }
    }
  }
}
