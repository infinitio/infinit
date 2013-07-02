#ifndef INFINIT_SATELLITE_HH
# define INFINIT_SATELLITE_HH

# include <functional>
# include <string>

# include <elle/Exception.hh>

namespace infinit
{
  class Exit:
    public elle::Exception
  {
  public:
    Exit(int value);

  private:
    ELLE_ATTRIBUTE_R(int, value);
  };

  int
  satellite_main(std::string const& name, std::function<void ()> const& action);
}

#endif
