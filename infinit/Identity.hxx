#ifndef INFINIT_IDENTITY_HXX
# define INFINIT_IDENTITY_HXX

# include <elle/serialize/Pointer.hh>

# include <cryptography/Code.hh>
# include <cryptography/Signature.hh>
# include <cryptography/KeyPair.hh>

ELLE_SERIALIZE_SIMPLE(infinit::Identity,
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
  archive & value.name;
  archive & elle::serialize::alive_pointer(value._signature);
}

#endif
