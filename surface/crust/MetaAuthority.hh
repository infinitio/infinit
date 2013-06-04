#ifndef METAAUTHORITY_HH
# define METAAUTHORITY_HH

# include <surface/crust/Authority.hh>
# include <plasma/meta/Client.hh>
# include <common/common.hh>

namespace infinit
{
  class MetaAuthority:
    public Authority
  {
    ELLE_ATTRIBUTE(plasma::meta::Client, meta);

  public:
    MetaAuthority(std::string const& host = common::meta::host(),
                  uint16_t port = common::meta::port());

    virtual
    cryptography::Signature
    sign(std::string const& hash) const override;

    virtual
    bool
    verify(std::string const& hash,
           cryptography::Signature const& signature) const override;
  };

}

#endif
