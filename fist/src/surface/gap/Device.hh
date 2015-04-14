#ifndef SURFACE_GAP_DEVICE_HH
# define SURFACE_GAP_DEVICE_HH

# include <string>

# include <elle/UUID.hh>

# include <surface/gap/Notification.hh>

namespace surface
{
  namespace gap
  {
    /// This class translates transactions so that headers aren't leaked into
    /// the GUI.
    class Device
      : public surface::gap::Notification
    {
    public:
      Device() = default;
      Device(elle::UUID const& id,
             std::string const& name,
             boost::optional<std::string> os,
             bool deleted = false);
      ~Device() noexcept(true);

      std::string id;
      std::string name;
      std::string os;
      bool deleted;

      static Notification::Type type;

    private:
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
