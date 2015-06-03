#ifndef SURFACE_GAP_PLAIN_INVITATION_HH
# define SURFACE_GAP_PLAIN_INVITATION_HH

# include <string>

# include <elle/Printable.hh>

namespace surface
{
  namespace gap
  {
    class PlainInvitation:
      elle::Printable
    {
    public:
      PlainInvitation() = default;
      PlainInvitation(std::string const& identifier,
                      std::string const& ghost_code,
                      std::string const& ghost_profile_url);
      ~PlainInvitation() = default;

      std::string identifier;
      std::string ghost_code;
      std::string ghost_profile_url;

    private:
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
