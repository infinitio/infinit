#include <surface/gap/Device.hh>

namespace surface
{
  namespace gap
  {
    Device::Device(elle::UUID const& id_,
                   std::string const& name_,
                   boost::optional<std::string> os_)
      : id(id_.repr())
      , name(name_)
      , os(os_ ? os_.get() : "")
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
  }
}
