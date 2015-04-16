#include <elle/log.hh>
#include <elle/serialize/TupleSerializer.hxx>

#include <cryptography/PublicKey.hh>
#include <cryptography/PrivateKey.hh>
#include <cryptography/KeyPair.hh>
#include <cryptography/Code.hh>
#include <cryptography/SecretKey.hh>

#include <papier/Authority.hh>

ELLE_LOG_COMPONENT("infinit.papier.Authority")

namespace papier
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

  Authority::Authority(cryptography::KeyPair const& pair):
    type(Authority::TypePair),
    _K(pair.K()),
    _k(new cryptography::PrivateKey{pair.k()}),
    _code(nullptr)
  {
  }

  Authority::Authority(cryptography::PublicKey const& K):
    type(Authority::TypePublic),
    _K(K),
    _k(nullptr),
    _code(nullptr)
  {
  }

  Authority::Authority(elle::io::Path const& path):
    _k(nullptr),
    _code(nullptr)
  {
    if (!papier::Authority::exists(path))
      throw elle::Exception
        (elle::sprintf("unable to locate the authority file %s", path));
    this->load(path);
  }

  Authority::Authority()
    : Authority(authority())
  {
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

  /*-----------------.
  | Global authority |
  `-----------------*/

  /// The Infinit authority public key which can be used to verify the
  /// authenticity of the various certificates populating the Infinit system.
  std::string const key("AAAAAAAAAAAAAgAAwvtjO51oHrOMK/K33ajUm4lnYKWW5dUtyK5Pih7gDrtlpEPy7QWAiYjY8Kinlctca2owzcPXrvFE34gxQE/xz11KLzw4ypn4/ABdzjaTgGoCNLJ2OX99IPk6sEjIHwFxR9YcewD6uED2FQgv4OfOROHaL8hmHzRc0/BxjKwtI6fT7Y/e1v2LMig6r30abqcLrZN+v+3rPHN4hb8n1Jz7kRxRbtglFPLDpvI5LUKEGmu3FPKWWZiJsyFuuLUoC9WsheDDZoHYjyrzMD0k7Sp5YVGBBDthZc6SQDp1ktPSTou1Opk+1IxHp/we1/HNhULvGvr6B1KYZJhb/R55H0k6GcaRQmNEKgiLcF6Z9lA5asK49CC/tlZjKRkXkLBKR9zGIODsweY+O9y3AeGX+Pfk9itPals2egsxc/q2mbRaC9svY2TXMwiSO4EPiedqvpuTKj1KTcRbQrp5mSxG1nzaCGyCmUeGzoBJZLNVJHpytAfwf0oYWfo9NOD9dkKkkL5jxfW3+MOwEx4i0tP3xdKmt6wC6CSXedFVm55oOcz2YgARG3hw0vBdLtl3jxfbXAFjCNnpkMrCEMfjzs5ecFVwhmM8OEPqHpyYJYO/9ipwXAKRPugFzMvoyggSA6G5JfVEwuCgOp2v82ldsKl0sC34/mBeKrJvjaZAXm39f6jTw/sAAAMAAAABAAE=");

  static
  papier::Authority
  _authority()
  {
    cryptography::PublicKey K;

    assert(!key.empty());
    if (K.Restore(key) == elle::Status::Error)
      throw elle::Exception("unable to restore the authority's public key");
    return papier::Authority(K);
  }

  papier::Authority
  authority()
  {
    static papier::Authority authority(_authority());
    return authority;
  }
}
