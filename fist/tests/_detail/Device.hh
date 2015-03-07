#ifndef FIST_SURFACE_GAP_TESTS_DEVICE_HH
# define FIST_SURFACE_GAP_TESTS_DEVICE_HH

# include <cryptography/KeyPair.hh>

# include <papier/Passport.hh>

# include <elle/UUID.hh>

namespace tests
{
  class Device
  {
  public:
    typedef elle::UUID Id;
  public:
    Device(cryptography::PublicKey const& key,
           boost::optional<elle::UUID> device);
  private:
    ELLE_ATTRIBUTE_R(Id, id);
    ELLE_ATTRIBUTE_R(papier::Passport, passport);

  public:
    std::string
    json() const;
  };
}

#endif
