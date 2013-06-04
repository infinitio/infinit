#ifndef SURFACE_CRUST_AUTHORITY_HH
# define SURFACE_CRUST_AUTHORITY_HH

# include <elle/serialize/Base64Archive.hh>
# include <elle/serialize/insert.hh>
# include <elle/serialize/extract.hh>

# include <cryptography/Signature.hh>

# include <string>

namespace infinit
{
  class Authority
  {
  public:
    /// Sign a hash.
    virtual
    cryptography::Signature
    sign(std::string const& hash) const = 0;

    /// Verify the signature of a hash.
    virtual
    bool
    verify(std::string const& hash,
           cryptography::Signature const& signature) const = 0;

    /// Sign a serializable object.
    template <typename T>
    cryptography::Signature
    sign(T const& serializable) const;

    /// Verify the signature of a serializable object.
    template <typename T>
    bool
    verify(T const& serializable,
           cryptography::Signature const& signature) const;
  };
}

# include "Authority.hxx"

#endif
