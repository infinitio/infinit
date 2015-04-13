#ifndef SURFACE_GAP_DEVICE_HH
# define SURFACE_GAP_DEVICE_HH

# include <string>

# include <elle/UUID.hh>

# include <das/Variable.hh>

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
             boost::optional<std::string> os);
      ~Device() noexcept(true);

      elle::UUID id;
      das::Variable<std::string> name;
      std::string os;

    private:
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
