#ifndef LUNE_IDENTITY_HH
# define LUNE_IDENTITY_HH

# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/concept/Fileable.hh>
# include <elle/concept/Uniquable.hh>
# include <elle/fwd.hh>

# include <cryptography/fwd.hh>
# include <cryptography/KeyPair.hh>
# include <cryptography/Signature.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <lune/fwd.hh>

namespace lune
{

  ///
  /// this class represents an identity issued by the Infinit authority
  /// which represents a user.
  ///
  /// note that the name attribute is supposed to be unique as it plays the
  /// role of identifier.
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
    ELLE_ATTRIBUTE_R(elle::String, id);
  public: // XXX
    elle::String                name;
  private: // XXX
    ELLE_ATTRIBUTE(cryptography::KeyPair*, pair);
    ELLE_ATTRIBUTE(cryptography::Signature*, signature);
  public: // XXX
    cryptography::Code*       code;

    ELLE_OPERATOR_NO_ASSIGNMENT(Identity);

  public:
    // XXX
    cryptography::KeyPair const&
    pair() const
    {
      ELLE_ASSERT(this->_pair != nullptr);

      return (*this->_pair);
    }

  public:
    Identity();
    Identity(std::string const& user_id);
    Identity(Identity const& other);
    ~Identity();

  public:
    elle::Status        Create(elle::String const&,
                               const elle::String&,
                               cryptography::KeyPair const&);

    elle::Status        Encrypt(const elle::String&);
    elle::Status        Decrypt(const elle::String&);

    elle::Status        Clear();

    elle::Status        Seal(elle::Authority const&);
    elle::Status        Validate(elle::Authority const&) const;

  private:
    /// XXX
    static
    std::string
    _path(elle::String const& user);

    //
    // interfaces
    //
  public:
    // dumpable
    elle::Status        Dump(const elle::Natural32 = 0) const;

    // fileable
    ELLE_CONCEPT_FILEABLE_METHODS();

    void
    load(elle::String const& user_id);

    void
    store() const;

    static
    void
    erase(elle::String const& user_id);

    static
    elle::Boolean
    exists(elle::String const& user_id);

    ELLE_SERIALIZE_FRIEND_FOR(Identity);
  };

}

#include <lune/Identity.hxx>

#endif
