#ifndef INFINIT_HOLE_EXCEPTION_HH
# define INFINIT_HOLE_EXCEPTION_HH

# include <elle/types.hh>
# include <elle/Exception.hh>

namespace hole
{
  /// Represent an exception triggered following an hole operation.
  class Exception:
    public elle::Exception
  {
    /*-------------.
    | Construction |
    `-------------*/
  public:
    Exception(elle::String const& message);
  };
}

#endif
