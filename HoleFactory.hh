#ifndef HOLEFACTORY_HH
# define HOLEFACTORY_HH

# include <elle/fwd.hh>

# include <hole/Hole.hh>
# include <hole/fwd.hh>

namespace infinit
{
  std::unique_ptr<hole::Hole>
  hole_factory(hole::storage::Storage& storage,
               hole::Passport const& passport,
               cryptography::PublicKey const& authority_K);
}

#endif
