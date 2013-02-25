#ifndef NUCLEUS_NEUTRON_TOKEN_HXX
# define NUCLEUS_NEUTRON_TOKEN_HXX

# include <cryptography/PrivateKey.hh>
# include <cryptography/PublicKey.hh>
# include <cryptography/SecretKey.hh>
# include <cryptography/Code.hh>

# include <nucleus/Exception.hh>

namespace nucleus
{
  namespace neutron
  {

    /*-------------.
    | Construction |
    `-------------*/

    template <typename T>
    Token::Token(T const& secret,
                 cryptography::PublicKey const& K):
      _type(Type::valid),
      _valid(new Valid{K.encrypt(secret)})
    {
    }

    /*--------.
    | Methods |
    `--------*/

    template <typename T>
    T
    Token::extract(cryptography::PrivateKey const& k) const
    {
      ELLE_ASSERT(this->_valid != nullptr);

      // Decrypt the code, revealing the secret information.
      T secret{k.decrypt<T>(this->_valid->code())};

      return (secret);
    }

  }
}

//
// ---------- serialize -------------------------------------------------------
//

# include <elle/serialize/Pointer.hh>

ELLE_SERIALIZE_SIMPLE(nucleus::neutron::Token,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & value._type;
  switch (value._type)
    {
    case nucleus::neutron::Token::Type::null:
      {
        break;
      }
    case nucleus::neutron::Token::Type::valid:
      {
        archive & elle::serialize::alive_pointer(value._valid);

        break;
      }
    default:
      throw Exception(elle::sprintf("unknown token type '%s'", value._type));
    }
}

ELLE_SERIALIZE_SIMPLE(nucleus::neutron::Token::Valid,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & value._code;
}

#endif
