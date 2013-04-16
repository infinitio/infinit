#include <nucleus/neutron/Component.hh>
#include <nucleus/Exception.hh>

#include <elle/printf.hh>

#include <iostream>
#include <stdexcept>

namespace nucleus
{
  namespace neutron
  {

    std::ostream&
    operator <<(std::ostream& stream,
                Component const component)
    {
      switch (component)
        {
        case ComponentUnknown:
          {
            stream << "unknown";
            break;
          }
        case ComponentObject:
          {
            stream << "object";
            break;
          }
        case ComponentContents:
          {
            stream << "contents";
            break;
          }
        case ComponentGroup:
          {
            stream << "group";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown component: '%s'",
                                          static_cast<int>(component)));
          }
        }

      return (stream);
    }

  }
}
