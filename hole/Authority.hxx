#ifndef  LUNE_AUTHORITY_HXX
# define LUNE_AUTHORITY_HXX

# include <cassert>
# include <iostream>

# include <elle/serialize/Pointer.hh>

ELLE_SERIALIZE_SIMPLE(elle::Authority,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & value.type;
  archive & value._K;
  archive & elle::serialize::alive_pointer(value.code);
}

#endif
