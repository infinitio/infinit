#include <horizon/Policy.hh>

#include <elle/printf.hh>
#include <elle/Exception.hh>

#include <map>
#include <string>

namespace horizon
{
  static const std::map<Policy, std::string> policy_to_string =
  {
    {Policy::accessible, "accessible"},
    {Policy::editable, "editable"},
    {Policy::confidential, "confidential"},
  };

  std::ostream&
  operator <<(std::ostream& stream, Policy policy)
  {
    return stream << policy_to_string.at(policy);
  }

  Policy
  policy_from_name(std::string const& name)
  {
    for (auto pair: policy_to_string)
      if (pair.second == name)
        return pair.first;
    throw elle::Exception(elle::sprintf("unknown policy name: %s", name));
  }
}
