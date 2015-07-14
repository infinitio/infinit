#include "Device.hh"

#include <elle/UUID.hh>
#include <elle/log.hh>

#include <fist/tests/_detail/Authority.hh>

// ELLE_LOG_COMPONENT("fist.tests")

namespace tests
{
  Device::Device(infinit::cryptography::rsa::PublicKey const& key,
                   boost::optional<elle::UUID> device)
    : _id(device ? device.get() : elle::UUID::random())
    , _passport(boost::lexical_cast<std::string>(this->_id), "osef", key, tests::authority)
  {
  }

  std::string
  Device::json() const
  {
    std::string passport_string;
    if (this->_passport.Save(passport_string) == elle::Status::Error)
      throw std::runtime_error("unabled to save the passport");
    return elle::sprintf(
      "{"
      "  \"id\" : \"%s\","
      "  \"name\": \"device\","
      "  \"passport\": \"%s\""
      "}",
      this->id(),
      passport_string);
  }
}
