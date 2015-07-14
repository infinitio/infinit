#ifndef PAPIER_IDENTITY_HH
# define PAPIER_IDENTITY_HH

# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/concept/Fileable.hh>
# include <elle/concept/Uniquable.hh>
# include <elle/fwd.hh>

# include <cryptography/fwd.hh>

# include <papier/Authority.hh>

namespace papier
{

  ///
  /// this class represents an identity issued by the Infinit authority
  /// which represents a user.
  ///
  class Identity:
    public elle::concept::MakeFileable<Identity>,
    public elle::concept::MakeUniquable<Identity>
  {
  public:
    // XXX
    static elle::Natural32 const keypair_length = 2048;

    enum Mode
      {
        ModeEncrypted,
        ModeUnencrypted
      };

  private:
    // FIXME: make this a boost::uuids::uuid.
    // The unique identifier of the identity.
    ELLE_ATTRIBUTE_R(elle::String, id);
  public:
    // The description of the identity. Cosmetic purpose only.
    ELLE_ATTRIBUTE_R(elle::String, description);
  private: // XXX
    ELLE_ATTRIBUTE(infinit::cryptography::rsa::KeyPair*, pair);
    ELLE_ATTRIBUTE(infinit::cryptography::Signature*, signature);
  public: // XXX
    infinit::cryptography::Code*       code;

    ELLE_OPERATOR_NO_ASSIGNMENT(Identity);

  public:
    // XXX
    infinit::cryptography::rsa::KeyPair const&
    pair() const
    {
      ELLE_ASSERT(this->_pair != nullptr);

      return (*this->_pair);
    }

  public:
    Identity();
    Identity(elle::io::Path const& path);
    Identity(Identity const& other);
    ~Identity();

  public:
    elle::Status
    Create(elle::String const& id,
           elle::String const& description,
           infinit::cryptography::rsa::KeyPair const&);

    elle::Status
    Encrypt(const elle::String&);
    elle::Status
    Decrypt(const elle::String&);

    elle::Status
    Clear();

    elle::Status
    Seal(papier::Authority const&);
    elle::Status
    Validate(papier::Authority const&) const;

  private:

    //
    // interfaces
    //
  public:
    // dumpable
    elle::Status
    Dump(const elle::Natural32 = 0) const;

    ELLE_SERIALIZE_FRIEND_FOR(Identity);
  };

}

#include <papier/Identity.hxx>

#endif
