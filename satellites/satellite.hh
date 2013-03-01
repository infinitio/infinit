#ifndef INFINIT_SATELLITE_HH
# define INFINIT_SATELLITE_HH

# include <functional>
# include <string>

namespace infinit
{
  int
  satellite_main(std::string const& name, std::function<void ()> const& action);
}

#endif
