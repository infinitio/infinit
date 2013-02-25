#ifndef NUCLEUS_NEUTRON_RECORD_HXX
# define NUCLEUS_NEUTRON_RECORD_HXX

/*-------------.
| Serializable |
`-------------*/

# include <elle/serialize/Serializer.hh>

ELLE_SERIALIZE_SIMPLE(nucleus::neutron::Record,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & value._type;

  switch (value._type)
    {
    case nucleus::neutron::Record::Type::null:
      {
        break;
      }
    case nucleus::neutron::Record::Type::valid:
      {
        archive & elle::serialize::alive_pointer(value._valid);

        break;
      }
    default:
      throw Exception(elle::sprintf("unknown record type '%s'", value._type));
    }
}

ELLE_SERIALIZE_SIMPLE(nucleus::neutron::Record::Valid,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & value._subject;
  archive & value._permissions;
  archive & elle::serialize::pointer(value._token);
}

#endif
