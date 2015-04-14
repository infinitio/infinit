#ifndef FIST_SURFACE_GAP_TESTS_DEVICE_HH
# define FIST_SURFACE_GAP_TESTS_DEVICE_HH

# include <elle/UUID.hh>
# include <elle/Printable.hh>

# include <cryptography/KeyPair.hh>

# include <papier/Passport.hh>

namespace tests
{
  class Device
    : public elle::Printable
  {
  public:
    Device(cryptography::PublicKey const& key,
           boost::optional<elle::UUID> device);
  private:
    ELLE_ATTRIBUTE_R(elle::UUID, id);
    ELLE_ATTRIBUTE_R(papier::Passport, passport);

  public:
    std::string
    json() const;

  public:
    void
    print(std::ostream& out) const override;
  };
}

#endif
