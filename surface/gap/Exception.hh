#ifndef SURFACE_GAP_EXCEPTION_HH
# define SURFACE_GAP_EXCEPTION_HH

# include <surface/gap/status.hh>

# include <string>
# include <stdexcept>

namespace surface
{
  namespace gap
  {
    struct Exception:
      public std::runtime_error
    {
    public:
      gap_Status const code;

    public:
      Exception(gap_Status code, std::string const& msg):
        std::runtime_error{msg},
        code{code}
      {}
    };
  }
}

#endif
