#ifndef NOTIFICATION_HH
# define NOTIFICATION_HH

# include <stdint.h>

namespace surface
{
  namespace gap
  {
    class Notification
    {
    public:
      typedef uint32_t Type;

    public:
      virtual
      ~Notification() = default;
    };
  }
}


#endif
