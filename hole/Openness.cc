#include <hole/Openness.hh>

#include <elle/Exception.hh>
#include <elle/printf.hh>

#include <iostream>
#include <map>
#include <string>

namespace hole
{
  static std::map<Openness, std::string> openness_to_string =
  {
    {Openness::open, "open"},
    {Openness::community, "community"},
    {Openness::closed, "closed"},
  };

  std::ostream&
  operator <<(std::ostream& stream, Openness openness)
  {
    return stream << openness_to_string.at(openness);
  }

  Openness
  openness_from_name(std::string const& name)
  {
    for (auto pair: openness_to_string)
      if (pair.second == name)
        return pair.first;
    throw elle::Exception(elle::sprintf("unknown model name: %s", name));
  }
}
