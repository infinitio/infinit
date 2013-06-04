#include "MetaAuthority.hh"

namespace infinit
{
  MetaAuthority::MetaAuthority(std::string const& host,
                               uint16_t port):
    _meta{host, port}
  {}

  cryptography::Signature
  MetaAuthority::sign(std::string const& hash) const
  {
    auto serialized_signature = this->_meta.sign_hash(hash).signature;
    cryptography::Signature signature;

    using namespace elle::serialize;
    from_string<InputBase64Archive>(serialized_signature) >> signature;

    return signature;
  }

  bool
  MetaAuthority::verify(std::string const& hash,
                        cryptography::Signature const& signature) const
  {
    std::string serialized_signature;

    using namespace elle::serialize;
    to_string<OutputBase64Archive>(serialized_signature) << signature;

    return this->_meta.verify_signature(serialized_signature, hash).verified;
  }
}
