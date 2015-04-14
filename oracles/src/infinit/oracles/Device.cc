#include <infinit/oracles/Device.hh>


namespace infinit
{
  namespace oracles
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
      s.serialize("name", this->name);
      s.serialize("os", this->os);
      s.serialize("passport", this->passport);
      s.serialize("online", this->_connected);
    }

    bool
    Device::connected() const
    {
      if (this->_connected)
        return this->_connected.get();
      return false;
    }

    void
    Device::print(std::ostream& stream) const
    {
      stream << "Device(" << this->id << ", " << this->name << ")";
    }

    bool
    Device::operator ==(Device const& other) const
    {
      return this->id == other.id &&
        this->name == other.name &&
        this->os == other.os &&
        this->last_sync == other.last_sync &&
        this->passport == other.passport &&
        this->connected() == other.connected();
    }

    bool
    Device::operator !=(Device const& other) const
    {
      return !(*this == other);
    }

    bool
    Device::operator <(Device const& other) const
    {
      return this->id < other.id;
    }
  }
}
