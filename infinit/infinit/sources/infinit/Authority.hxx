#ifndef INFINIT_AUTHORITY_HXX
# define INFINIT_AUTHORITY_HXX

# include <infinit/Exception.hh>

/*-------------.
| Serializable |
`-------------*/

ELLE_SERIALIZE_STATIC_FORMAT(infinit::Authority, 1);

ELLE_SERIALIZE_SPLIT(infinit::Authority);

ELLE_SERIALIZE_SPLIT_SAVE(infinit::Authority,
                          archive,
                          value,
                          format)
{
  switch (format)
  {
    case 0:
    {
      // First, serialize the type TypePair so as to indicate
      // the instance actually represents an authority with a key pair.
      enum Type
      {
        TypeUnknown,
        TypePair,
        TypePublic
      };
      archive << TypePair;

      archive << value._K;

      cryptography::Code const* _k(&value._k);
      archive << elle::serialize::alive_pointer(_k);

      break;
    }
    case 1:
    {
      archive << value._identifier;
      archive << value._K;
      archive << value._description;
      archive << value._k;

      break;
    }
    default:
      throw ::infinit::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

ELLE_SERIALIZE_SPLIT_LOAD(infinit::Authority,
                          archive,
                          value,
                          format)
{
  switch (format)
  {
    case 0:
    {
      // Extract an enumeration value which used to indicate whether
      // the instance was an authority or a kind-of-certificate.
      enum Type
      {
        TypeUnknown,
        TypePair,
        TypePublic
      };
      Type type;
      archive >> type;
      (void)type;

      archive >> value._K;
      archive >> value._k;

      break;
    }
    case 1:
    {
      archive >> value._identifier;
      archive >> value._K;
      archive >> value._description;
      archive >> value._k;

      break;
    }
    default:
      throw ::infinit::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

#endif
