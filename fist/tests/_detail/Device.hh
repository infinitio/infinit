#ifndef FIST_SURFACE_GAP_TESTS_DEVICE_HH
# define FIST_SURFACE_GAP_TESTS_DEVICE_HH

# include <elle/UUID.hh>

# include <cryptography/rsa/KeyPair.hh>
# include <infinit/oracles/meta/Device.hh>

# include <papier/Passport.hh>

namespace tests
{
  class Device
    : public infinit::oracles::meta::Device
  {
  public:
    Device(infinit::cryptography::rsa::PublicKey const& key,
           boost::optional<elle::UUID> device_id);
  private:
    ELLE_ATTRIBUTE_R(papier::Passport, passport_);

  public:
    std::string
    json() const;
  };
}

#endif
