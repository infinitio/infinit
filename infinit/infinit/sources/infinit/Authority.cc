#include <elle/log.hh>
#include <elle/serialize/TupleSerializer.hxx>
#include <elle/io/File.hh>

#include <cryptography/PublicKey.hh>
#include <cryptography/PrivateKey.hh>
#include <cryptography/KeyPair.hh>
#include <cryptography/Code.hh>
#include <cryptography/SecretKey.hh>

#include <hole/Authority.hh>
#include <hole/Exception.hh>

ELLE_LOG_COMPONENT("infinit.lune.Authority")

namespace elle
{
  /*-------------.
  | Construction |
  `-------------*/

  Authority::Authority(Authority const& from):
    type(from.type),
    _K(from._K),
    _k(nullptr),
    _code(nullptr)
  {
    if (from._k)
      this->_k = new cryptography::PrivateKey(*from._k);
    if (from._code)
      this->_code = new cryptography::Code(*from._code);
  }

  Authority::Authority(cryptography::KeyPair const& keypair):
    type(Authority::TypePair),
    _K(keypair.K()),
    _k(new cryptography::PrivateKey{keypair.k()}),
    _code(nullptr)
  {}

  Authority::Authority(cryptography::PublicKey const& K):
    type(Authority::TypePublic),
    _K(K),
    _k(nullptr),
    _code(nullptr)
  {}

  Authority::Authority(elle::io::Path const& path):
    _k(nullptr),
    _code(nullptr)
  {
    if (!elle::Authority::exists(path))
      throw Exception
        (elle::sprintf("unable to locate the authority file %s", path));
    this->load(path);
  }

  Authority::~Authority()
  {
    delete this->_k;
    delete this->_code;
  }

//
// ---------- methods ---------------------------------------------------------
//

  /// this method encrypts the keys.
  ///
  elle::Status          Authority::Encrypt(const elle::String&          pass)
  {
    ELLE_TRACE_METHOD(pass);

    // XXX[setter l'algo en constant pour eviter la duplication avec decrypt()]
    cryptography::SecretKey key{cryptography::cipher::Algorithm::aes256, pass};

    ELLE_ASSERT(this->type == Authority::TypePair);

    delete this->_code;
    this->_code = nullptr;
    this->_code = new cryptography::Code{key.encrypt(this->k())};

    return elle::Status::Ok;
  }

  ///
  /// this method decrypts the keys.
  ///
  elle::Status          Authority::Decrypt(const elle::String&          pass)
  {
    ELLE_TRACE_METHOD(pass);

    ELLE_ASSERT(this->type == Authority::TypePair);
    ELLE_ASSERT(this->_code != nullptr);

    cryptography::SecretKey key{cryptography::cipher::Algorithm::aes256, pass};

    delete this->_k;
    this->_k = nullptr;
    this->_k =
      new cryptography::PrivateKey{
        key.decrypt<cryptography::PrivateKey>(*this->_code)};

    return elle::Status::Ok;
  }

//
// ---------- dumpable --------------------------------------------------------
//

  ///
  /// this method dumps a authority.
  ///
  elle::Status          Authority::Dump(const elle::Natural32   margin) const
  {
    elle::String        alignment(margin, ' ');
    elle::String        unique;

    std::cout << alignment << "[Authority]" << std::endl;

    // dump the type.
    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Type] " << this->type << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[K] " << this->_K << std::endl;

    // if present...
    if (this->_k != nullptr)
      {
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[k] " << this->k() << std::endl;
      }

    // dump the code.
    if (this->_code != nullptr)
      {
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Code] " << this->code() << std::endl;
      }

    return elle::Status::Ok;
  }

  cryptography::PrivateKey const&
  Authority::k() const
  {
    ELLE_ASSERT_NEQ(this->_k, nullptr);
    return *this->_k;
  }

  cryptography::Code const&
  Authority::code() const
  {
    ELLE_ASSERT_NEQ(this->_code, nullptr);
    return *this->_code;
  }
}
