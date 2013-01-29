#include <cryptography/PrivateKey.hh>
#include <cryptography/Seed.hh>
#include <cryptography/cryptography.hh>
#include <cryptography/Code.hh>

#include <elle/log.hh>

#include <comet/Comet.hh>

ELLE_LOG_COMPONENT("infinit.cryptography.PrivateKey");

namespace infinit
{
  namespace cryptography
  {
    /*-------------.
    | Construction |
    `-------------*/

    PrivateKey::PrivateKey()
    {
      // Make sure the cryptographic system is set up.
      cryptography::require();
    }

    PrivateKey::PrivateKey(std::unique_ptr<interface::PrivateKey>&& impl):
      _implementation(std::move(impl))
    {
      // Make sure the cryptographic system is set up.
      cryptography::require();
    }

    PrivateKey::PrivateKey(PrivateKey const& other):
      _implementation(other._implementation->clone())
    {
      // Make sure the cryptographic system is set up.
      cryptography::require();
    }

    PrivateKey::PrivateKey(PrivateKey&& other):
      _implementation(std::move(other._implementation))
    {
      // Make sure the cryptographic system is set up.
      cryptography::require();
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(PrivateKey)
    {
      // Make sure the cryptographic system is set up.
      cryptography::require();
    }

    /*--------.
    | Methods |
    `--------*/

    Clear
    PrivateKey::decrypt(Code const& code) const
    {
      ELLE_TRACE_METHOD(code);

      ELLE_ASSERT(this->_implementation != nullptr);

      return (this->_implementation->decrypt(code));
    }

    Signature
    PrivateKey::sign(Plain const& plain) const
    {
      ELLE_TRACE_METHOD(plain);

      ELLE_ASSERT(this->_implementation != nullptr);

      return (this->_implementation->sign(plain));
    }

    Code
    PrivateKey::encrypt(Plain const& plain) const
    {
      ELLE_TRACE_METHOD(plain);

      ELLE_ASSERT(this->_implementation != nullptr);

      return (this->_implementation->encrypt(plain));
    }

    /*----------.
    | Operators |
    `----------*/

    elle::Boolean
    PrivateKey::operator ==(PrivateKey const& other) const
    {
      if (this == &other)
        return (true);

      ELLE_ASSERT(this->_implementation != nullptr);
      ELLE_ASSERT(other._implementation != nullptr);

      return (*this->_implementation == *other._implementation);
    }

    elle::Boolean
    PrivateKey::operator <(PrivateKey const& other) const
    {
      if (this == &other)
        return (true);

      ELLE_ASSERT(this->_implementation != nullptr);
      ELLE_ASSERT(other._implementation != nullptr);

      return (*this->_implementation < *other._implementation);
    }

    /*----------.
    | Printable |
    `----------*/

    void
    PrivateKey::print(std::ostream& stream) const
    {
      ELLE_ASSERT(this->_implementation != nullptr);

      this->_implementation->print(stream);
    }
  }
}
