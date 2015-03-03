#ifndef NOTIFICATION_HH
# define NOTIFICATION_HH

# include <stdint.h>

# include <elle/Printable.hh>

namespace surface
{
  namespace gap
  {
    class Notification
      : public elle::Printable
    {
    public:
      typedef uint32_t Type;
      virtual
      ~Notification() = default;

    protected:
      virtual
      void
      print(std::ostream& output) const override;
    };
  }
}

#endif
