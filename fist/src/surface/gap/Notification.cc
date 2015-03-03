#include <surface/gap/Notification.hh>
#include <elle/printf.hh>

namespace surface
{
  namespace gap
  {
    void
    Notification::print(std::ostream& output) const
    {
      elle::fprintf(output, "surface::gap::Notification(%s)", this);
    }
  }
}
