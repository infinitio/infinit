#ifndef INFINIT_IDENTIFIER_HXX
# define INFINIT_IDENTIFIER_HXX

# include <elle/serialize/Serializer.hh>

# include <string>

namespace std
{
  /*---------------------.
  | Type Specializations |
  `---------------------*/

  template <>
  struct hash<::infinit::Identifier>
  {
    size_t
    operator ()(::infinit::Identifier const& identifier) const
    {
      std::hash<std::string> hash_function;

      return (hash_function(identifier.value()));
    }
  };
}

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
