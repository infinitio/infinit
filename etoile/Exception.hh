#ifndef INFINIT_ETOILE_EXCEPTION_HH
# define INFINIT_ETOILE_EXCEPTION_HH

# include <elle/types.hh>
# include <elle/Exception.hh>

namespace etoile
{
  /// Represent an exception triggered following an etoile operation.
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
