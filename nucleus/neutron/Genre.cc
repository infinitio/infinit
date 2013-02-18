#include <iostream>

#include <elle/printf.hh>

#include <nucleus/neutron/Genre.hh>
#include <nucleus/Exception.hh>

namespace nucleus
{
  namespace neutron
  {
    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Genre const genre)
    {
      switch (genre)
        {
        case Genre::file:
          {
            stream << "file";
            break;
          }
        case Genre::directory:
          {
            stream << "directory";
            break;
          }
        case Genre::link:
          {
            stream << "link";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown genre '%s'",
                                          static_cast<int>(genre)));
          }
        }

      return (stream);
    }
  }
}
