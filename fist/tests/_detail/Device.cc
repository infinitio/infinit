#include "Device.hh"

#include <fist/tests/_detail/Authority.hh>


# include <elle/log.hh>

ELLE_LOG_COMPONENT("fist.tests");

namespace tests
{
  Device::Device(cryptography::PublicKey const& key,
                   boost::optional<boost::uuids::uuid> device)
    : _id(device ? device.get() : boost::uuids::random_generator()())
    , _passport(boost::lexical_cast<std::string>(this->_id), "osef", key, authority)
  {
    ELLE_LOG("LLLLLLLLLLLLLLL %s", key);
  }

  std::string
  Device::json() const
  {
    std::string passport_string;
    if (this->_passport.Save(passport_string) == elle::Status::Error)
      throw std::runtime_error("unabled to save the passport");
ELLE_LOG("passport: %s", passport_string);
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
