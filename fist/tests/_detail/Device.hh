#ifndef FIST_SURFACE_GAP_TESTS_DEVICE_HH
# define FIST_SURFACE_GAP_TESTS_DEVICE_HH

# include <cryptography/KeyPair.hh>

# include <papier/Passport.hh>

# include <fist/tests/_detail/uuids.hh>

namespace tests
{
  class Device
  {
  public:
    typedef boost::uuids::uuid Id;
  public:
    Device(cryptography::PublicKey const& key,
           boost::optional<boost::uuids::uuid> device);
  private:
    ELLE_ATTRIBUTE_R(Id, id);
    ELLE_ATTRIBUTE_R(papier::Passport, passport);

  public:
    std::string
    json() const;
  };
}

#endif
