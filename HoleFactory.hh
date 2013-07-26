#ifndef HOLEFACTORY_HH
# define HOLEFACTORY_HH

# include <elle/fwd.hh>

# include <hole/Hole.hh>
# include <lune/fwd.hh>

namespace infinit
{
  std::unique_ptr<hole::Hole>
  hole_factory(papier::Descriptor const& descriptor,
               hole::storage::Storage& storage,
               papier::Passport const& passport,
               papier::Authority const& authority,
               std::vector<elle::network::Locus> const& members,
               std::string const& host = "",
               uint16_t port = 0, // XXX: Ugly, we should use boost::optional.
               std::string const& token = "");
}

#endif
