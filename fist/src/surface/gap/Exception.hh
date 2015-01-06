#ifndef SURFACE_GAP_EXCEPTION_HH
# define SURFACE_GAP_EXCEPTION_HH

# include <surface/gap/enums.hh>

# include <elle/Exception.hh>

# include <string>
# include <stdexcept>

namespace surface
{
  namespace gap
  {
    struct Exception:
      public elle::Exception
    {
    public:
      gap_Status const code;

    public:
      Exception(gap_Status code, std::string const& msg):
        elle::Exception{msg},
        code{code}
      {
      }
    };
  }
}

#endif
