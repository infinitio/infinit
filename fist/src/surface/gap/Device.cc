#include <surface/gap/Device.hh>
#include <surface/gap/enums.hh>

namespace surface
{
  namespace gap
  {
    Device::Device(elle::UUID const& id_,
                   std::string const& name_,
                   boost::optional<std::string> os_,
                   bool deleted)
      : id(id_.repr())
      , name(name_)
      , os(os_ ? os_.get() : "")
      , deleted(deleted)
    {}

    Device::~Device() noexcept(true)
    {}

    void
    Device::print(std::ostream& stream) const
    {
      stream << "Device("
             << this->id << ", "
             << "name: " << this->name << ", "
             << "os: " << this->os << ")";
    }

    Notification::Type Device::type = NotificationType_DeviceUpdated;
  }
}
