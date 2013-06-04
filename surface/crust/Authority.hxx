#ifndef AUTHORITY_HXX
# define AUTHORITY_HXX

namespace infinit
{
  template <typename T>
  cryptography::Signature
  Authority::sign(T const& serializable) const
  {
    using namespace elle::serialize;

    std::string serialized;
    to_string<OutputBase64Archive>(serialized) << serializable;

    return this->sign(serialized);
  }

  template <typename T>
  bool
  Authority::verify(T const& serializable,
                    cryptography::Signature const& signature) const
  {
    using namespace elle::serialize;

    std::string serialized;
    to_string<OutputBase64Archive>(serialized) << serializable;

    return this->verify(serialized, signature);
  }
}


#endif
