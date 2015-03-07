#ifndef SURFACE_GAP_USER_HH
# define SURFACE_GAP_USER_HH

# include <stdint.h>
# include <string>

# include <boost/optional.hpp>

# include <elle/Printable.hh>

# include <surface/gap/enums.hh>
# include <surface/gap/Notification.hh>

namespace surface
{
  namespace gap
  {
    /// This class translates transactions so that headers aren't leaked into
    /// the GUI.
    class User
      : public surface::gap::Notification
    {
    public:
      User() = default;
      User(uint32_t id,
           bool status,
           std::string const& fullname,
           std::string const& handle,
           std::string const& meta_id,
           bool swagger,
           bool deleted,
           bool ghost,
           boost::optional<std::string> ghost_code = {},
           boost::optional<std::string> ghost_invitation_url = {});
      ~User() noexcept(true);

      uint32_t id;
      bool status;
      std::string fullname;
      std::string handle;
      std::string meta_id;
      bool swagger;
      bool deleted;
      bool ghost;
      std::string ghost_code;
      std::string ghost_invitation_url;


      static Notification::Type type;

      private:
      void
      print(std::ostream& stream) const override;

    };
  }
}

#endif
