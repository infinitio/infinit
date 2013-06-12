#ifndef INFINIT_EXCEPTION_HH
# define INFINIT_EXCEPTION_HH

# include <elle/types.hh>
# include <elle/Exception.hh>

namespace infinit
{
  class Exception:
    public elle::Exception
  {
  public:
    Exception(elle::String const& message);
  };
}

#endif
