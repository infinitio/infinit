#ifndef PAPIER_IDENTITY_HXX
# define PAPIER_IDENTITY_HXX

# include <elle/serialize/Pointer.hh>

# include <cryptography/_legacy/Code.hh>
# include <cryptography/_legacy/Signature.hh>
# include <cryptography/rsa/KeyPair.hh>

ELLE_SERIALIZE_SIMPLE(papier::Identity,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & elle::serialize::pointer(value.code);

  // XXX[way of serializing the identity in its decrypted form:
  //     not very elegant: to improve!]
  if (value.code != nullptr)
    archive & elle::serialize::alive_pointer(value._pair);

  archive & value._id;
  archive & value._description;
  archive & elle::serialize::alive_pointer(value._signature);
}

#endif
