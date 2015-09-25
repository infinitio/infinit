#include "Device.hh"

#include <elle/log.hh>
#include <elle/serialization/json.hh>
#include <elle/UUID.hh>

#include <fist/tests/_detail/Authority.hh>

// ELLE_LOG_COMPONENT("fist.tests")

namespace tests
{
  Device::Device(infinit::cryptography::rsa::PublicKey const& key,
                 boost::optional<elle::UUID> device_id)
    : infinit::oracles::meta::Device()
    , _passport_(boost::lexical_cast<std::string>(this->id),
                 "osef", key, tests::authority)
  {
    this->id = device_id ? device_id.get() : elle::UUID::random();
    this->name = elle::sprintf("device-name/%s", this->id);
    std::string passport_string;
    if (this->_passport_.Save(passport_string) == elle::Status::Error)
      throw std::runtime_error("unabled to save the passport");
    this->passport = passport_string;
  }

  std::string
  Device::json() const
  {
    std::stringstream ss;
    {
      elle::serialization::json::SerializerOut output(ss, false);
      auto meta_device = static_cast<infinit::oracles::meta::Device>(*this);
      meta_device.serialize(output);
    }
    return ss.str();
  }
}
