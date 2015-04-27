#include <infinit/oracles/meta/Device.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      Device::Device(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Device::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("id", this->id);
        s.serialize("last_sync", this->last_sync);
        s.serialize("model", this->model);
        s.serialize("name", this->name);
        s.serialize("os", this->os);
        s.serialize("passport", this->passport);
      }

      void
      Device::print(std::ostream& stream) const
      {
        stream << "Device(" << this->name << ")";
      }
    }
  }
}
