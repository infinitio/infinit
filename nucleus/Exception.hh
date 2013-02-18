#ifndef NUCLEUS_EXCEPTION_HH
# define NUCLEUS_EXCEPTION_HH

# include <elle/types.hh>
# include <elle/Exception.hh>

namespace nucleus
{
  class Exception:
    public elle::Exception
  {
  public:
    Exception(elle::String const& message);
  };
}

#endif
