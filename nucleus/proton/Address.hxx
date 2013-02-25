#ifndef NUCLEUS_PROTON_ADDRESS_HXX
# define NUCLEUS_PROTON_ADDRESS_HXX

# include <elle/assert.hh>
# include <elle/serialize/TupleSerializer.hxx>
# include <elle/serialize/Serializer.hh>
# include <elle/serialize/Pointer.hh>

# include <nucleus/Exception.hh>

# include <tuple>

namespace nucleus
{
  namespace proton
  {

    template <typename... T>
    Address::Address(Network const& network,
                     const Family& family,
                     const neutron::Component& component,
                     const T&... parameters):
      _type(Type::valid),
      _valid(new Address::Valid(network,
                                family,
                                component,
                                cryptography::oneway::hash(
                                  elle::serialize::make_tuple(
                                    network,
                                    family,
                                    component,
                                    parameters...),
                                  Address::Constants::oneway_algorithm)))
    {
    }

  }
}

//
// ---------- serialize -------------------------------------------------------
//

# include <cryptography/Digest.hh>

ELLE_SERIALIZE_SIMPLE(nucleus::proton::Address,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & value._type;

  switch (value._type)
    {
    case nucleus::proton::Address::Type::null:
      {
        break;
      }
    case nucleus::proton::Address::Type::valid:
      {
        archive & elle::serialize::alive_pointer(value._valid);

        break;
      }
    default:
      throw Exception(elle::sprintf("unknown address type '%s'", value._type));
    }
}

ELLE_SERIALIZE_SIMPLE(nucleus::proton::Address::Valid,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & value._network;
  archive & value._family;
  archive & value._component;
  archive & value._digest;
}

#endif
