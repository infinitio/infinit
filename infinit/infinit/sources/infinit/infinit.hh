#ifndef INFINIT_INFINIT_HH
# define INFINIT_INFINIT_HH

# include <infinit/Certificate.hh>

namespace infinit
{
  namespace certificate
  {
    /*----------.
    | Functions |
    `----------*/

    /// Return the certificate of the root authority from which every entity
    /// should have been signed.
    Certificate const&
    origin();
  }
}

#endif
