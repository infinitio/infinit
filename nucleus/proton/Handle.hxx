#ifndef NUCLEUS_PROTON_HANDLE_HXX
# define NUCLEUS_PROTON_HANDLE_HXX

/*-------------.
| Serializable |
`-------------*/

# include <elle/serialize/Pointer.hh>

# include <cryptography/SecretKey.hh>

# include <nucleus/proton/Clef.hh>
# include <nucleus/proton/Egg.hh>

ELLE_SERIALIZE_SPLIT(nucleus::proton::Handle);

ELLE_SERIALIZE_SPLIT_SAVE(nucleus::proton::Handle,
                          archive,
                          value,
                          version)
{
  enforce(version == 0);

  switch (value._phase)
    {
    case nucleus::proton::Handle::Phase::unnested:
      {
        ELLE_ASSERT(value._clef != nullptr);

        archive << elle::serialize::alive_pointer(value._clef);

        break;
      }
    case nucleus::proton::Handle::Phase::nested:
      {
        ELLE_ASSERT(value._egg != nullptr);
        ELLE_ASSERT(*value._egg != nullptr);

        // In this case, extract the clef from the egg and serialize it.
        archive << (*value._egg)->alive();

        break;
      }
    default:
      throw Exception(elle::sprintf("unknown phase '%s'", value._phase));
    }
}

ELLE_SERIALIZE_SPLIT_LOAD(nucleus::proton::Handle,
                          archive,
                          value,
                          version)
{
  enforce(version == 0);

  ELLE_ASSERT(value._phase == nucleus::proton::Handle::Phase::unnested);
  ELLE_ASSERT(value._clef == nullptr);

  value._clef = new nucleus::proton::Clef{archive};
}

#endif
