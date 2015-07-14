#include <elle/os/path.hh>
#include <elle/serialize/TupleSerializer.hxx>

#include <cryptography/rsa/KeyPair.hh>
#include <cryptography/_legacy/Code.hh>
#include <cryptography/SecretKey.hh>
#include <cryptography/_legacy/Signature.hh>

#include <papier/Authority.hh>

#include <papier/Identity.hh>

namespace path = elle::os::path;

namespace papier
{

//  ELLE_LOG_COMPONENT("infinit.papier.Identity");

//
// ---------- constructors & destructors --------------------------------------
//

  ///
  /// default constructor.
  ///
  Identity::Identity():
    _pair(nullptr),
    _signature(nullptr),
    code(nullptr)
  {}

  Identity::Identity(elle::io::Path const& path):
    Identity()
  {
    this->load(path);
  }

  Identity::Identity(Identity const& other):
    _id(other._id),
    _description(other._description),
    _pair(new infinit::cryptography::rsa::KeyPair{*other._pair}),
    _signature(new infinit::cryptography::Signature{*other._signature}),
    code(new infinit::cryptography::Code{*other.code})
  {}

  ///
  /// destructor.
  ///
  Identity::~Identity()
  {
    delete this->_signature;
    delete this->_pair;
    delete this->code;
  }

//
// ---------- methods ---------------------------------------------------------
//

  ///
  /// this method creates an identity based on the given key pair.
  ///
  elle::Status
  Identity::Create(elle::String const& user_id,
                   const elle::String& description,
                   infinit::cryptography::rsa::KeyPair const& pair)
  {
    this->_id = user_id;
    this->_description = description;

    delete this->_pair;
    this->_pair = nullptr;
    this->_pair = new infinit::cryptography::rsa::KeyPair{pair};

    return elle::Status::Ok;
  }

  ///
  /// this method encrypts the key pair.
  ///
  elle::Status          Identity::Encrypt(const elle::String&   pass)
  {
    // XXX[factor algo with decrypt()]
    infinit::cryptography::SecretKey key(
      pass,
      infinit::cryptography::Cipher::aes256,
      infinit::cryptography::Mode::cbc);

    ELLE_ASSERT(this->_pair != nullptr);

    delete this->code;
    this->code = nullptr;
    this->code = new infinit::cryptography::Code{
      key.encrypt(*this->_pair)};

    return elle::Status::Ok;
  }

  ///
  /// this method decrypts the key pair.
  ///
  elle::Status          Identity::Decrypt(const elle::String&   pass)
  {
    // check the code.
    if (this->code == nullptr)
      throw elle::Exception("unable to decrypt an unencrypted identity");

    infinit::cryptography::SecretKey key{
      pass,
      infinit::cryptography::Cipher::aes256,
      infinit::cryptography::Mode::cbc};

    // decrypt the authority.
    delete this->_pair;
    this->_pair = nullptr;
    this->_pair = new infinit::cryptography::rsa::KeyPair{
      key.decrypt<infinit::cryptography::rsa::KeyPair>(*this->code)};

    return elle::Status::Ok;
  }

  ///
  /// this method clears the identity i.e removes the code.
  ///
  /// this is required for the Serialize() method to consider the identity
  /// in its unencrypted form.
  ///
  elle::Status          Identity::Clear()
  {
    if (this->code != nullptr)
      {
        delete this->code;
        this->code = nullptr;
      }

    return elle::Status::Ok;
  }

  ///
  /// this method seals the identity with the authority.
  ///
  elle::Status
  Identity::Seal(papier::Authority const& authority)
  {
    // check the code.
    if (this->code == nullptr)
      throw elle::Exception("unable to seal an unencrypted identity");

    // sign with the authority.
    delete this->_signature;
    this->_signature = nullptr;
    this->_signature = new infinit::cryptography::Signature{
      authority.k().sign(
        elle::serialize::make_tuple(this->_id, this->_description, *this->code))};

    return elle::Status::Ok;
  }

  ///
  /// this method verifies the validity of the identity.
  ///
  elle::Status
  Identity::Validate(papier::Authority const& authority)
    const
  {
    // check the code.
    if (this->code == nullptr)
      throw elle::Exception("unable to verify an unencrypted identity");

    // verify the signature.
    ELLE_ASSERT(this->_signature != nullptr);

    if (authority.K().verify(
          *this->_signature,
          elle::serialize::make_tuple(this->_id,
                                      this->_description,
                                      *this->code)) == false)
      throw elle::Exception("unable to verify the signature");

    return elle::Status::Ok;
  }

//
// ---------- dumpable --------------------------------------------------------
//

  ///
  /// this method dumps a identity.
  ///
  elle::Status          Identity::Dump(const elle::Natural32    margin) const
  {
    elle::String        alignment(margin, ' ');

    std::cout << alignment << "[Identity]" << std::endl;

    // dump the id.
    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Id] " << this->_id << std::endl;

    // dump the description.
    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Description] " << this->_description << std::endl;

    // dump the pair.
    if (this->_pair != nullptr)
      {
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Pair] " << *this->_pair << std::endl;
      }

    // dump the signature.
    if (this->_signature != nullptr)
      {
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Signature] " << *this->_signature << std::endl;
      }

    // dump the code.
    if (this->code != nullptr)
      {
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Code] " << *this->code << std::endl;
      }

    return elle::Status::Ok;
  }
}
