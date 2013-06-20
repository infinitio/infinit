#ifndef INFINIT_IDENTIFIER_HXX
# define INFINIT_IDENTIFIER_HXX

# include <elle/serialize/Serializer.hh>

/*-----------.
| Serializer |
`-----------*/

ELLE_SERIALIZE_SIMPLE(infinit::Identifier,
                      archive,
                      value,
                      format)
{
  enforce(format == 0, "unknown format");

  archive & value._value;
}

#endif
