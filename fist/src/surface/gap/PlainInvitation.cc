#include <surface/gap/PlainInvitation.hh>

namespace surface
{
  namespace gap
  {
    PlainInvitation::PlainInvitation(std::string const& identifier_,
                                     std::string const& ghost_code_,
                                     std::string const& ghost_profile_url_)
      : identifier(identifier_)
      , ghost_code(ghost_code_)
      , ghost_profile_url(ghost_profile_url_)
    {}

    void
    PlainInvitation::print(std::ostream& stream) const
    {
      stream << "PlainInvitation(" << this->identifier << ", "
             << this->ghost_code << ", " << this->ghost_profile_url << ")";
    }
  }
}