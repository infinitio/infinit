#include <infinit/Certificate.hh>
#include <infinit/Exception.hh>

#include <cryptography/KeyPair.hh>

#include <elle/serialize/TupleSerializer.hxx>
#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.Certificate")

namespace infinit
{
  namespace certificate
  {
    /*-------.
    | Values |
    `-------*/

    namespace permissions
    {
      Permissions const none = 0;
      Permissions const identity = (1 << 1);
      Permissions const descriptor = (1 << 2);
      Permissions const passport = (1 << 3);
      Permissions const certificate = (1 << 4);
      Permissions const all = identity | descriptor | passport | certificate;
    }
  }

  /*-------------.
  | Construction |
  `-------------*/

  Certificate::Certificate(cryptography::PublicKey issuer_K,
                           cryptography::PublicKey subject_K,
                           elle::String description,
                           certificate::Permissions permissions,
                           std::chrono::system_clock::time_point valid_from,
                           std::chrono::system_clock::time_point valid_until,
                           cryptography::Signature issuer_signature,
                           Identifier identifier):
    _issuer_K(std::move(issuer_K)),
    _identifier(std::move(identifier)),
    _subject_K(std::move(subject_K)),
    _description(std::move(description)),
    _permissions(std::move(permissions)),
    _valid_from(std::move(valid_from)),
    _valid_until(std::move(valid_until)),
    _issuer_signature(std::move(issuer_signature))
  {
  }

  Certificate::Certificate(Certificate const& other):
    _issuer_K(other._issuer_K),
    _identifier(other._identifier),
    _subject_K(other._subject_K),
    _description(other._description),
    _permissions(other._permissions),
    _valid_from(other._valid_from),
    _valid_until(other._valid_until),
    _issuer_signature(other._issuer_signature)
  {
  }

  Certificate::Certificate(Certificate&& other):
    _issuer_K(std::move(other._issuer_K)),
    _identifier(std::move(other._identifier)),
    _subject_K(std::move(other._subject_K)),
    _description(std::move(other._description)),
    _permissions(std::move(other._permissions)),
    _valid_from(std::move(other._valid_from)),
    _valid_until(std::move(other._valid_until)),
    _issuer_signature(std::move(other._issuer_signature))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Certificate,
                                  _issuer_K,
                                  _identifier,
                                  _subject_K,
                                  _issuer_signature)
  {
  }

  /*--------.
  | Methods |
  `--------*/

  elle::Boolean
  Certificate::validate(Pool const& pool) const
  {
    ELLE_TRACE_METHOD(pool);

    // XXX
    throw Exception("not implemented yet");
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Certificate::print(std::ostream& stream) const
  {
    stream << "("
           << this->_subject_K << ", "
           << this->_description << ", ";

    stream << "[";
    elle::Boolean first = true;

    if (this->_permissions & certificate::permissions::identity)
    {
      if (!first)
        stream << " | ";
      stream << "identity";
      first = false;
    }

    if (this->_permissions & certificate::permissions::descriptor)
    {
      if (!first)
        stream << " | ";
      stream << "descriptor";
      first = false;
    }

    if (this->_permissions & certificate::permissions::passport)
    {
      if (!first)
        stream << " | ";
      stream << "passport";
      first = false;
    }

    if (this->_permissions & certificate::permissions::certificate)
    {
      if (!first)
        stream << " | ";
      stream << "certificate";
      first = false;
    }
    stream << "]";

    stream << ")";
  }

  /*----------.
  | Operators |
  `----------*/

  std::ostream&
  operator <<(std::ostream& stream,
              Certificate::Pool const& pool)
  {
    elle::Boolean first = true;

    stream << "<";
    for (auto const& pair: pool)
    {
      auto const& certificate = pair.second;

      if (!first)
        stream << ", ";
      stream << certificate;
      first = false;
    }
    stream << ">";

    return (stream);
  }

  namespace certificate
  {
    /*----------.
    | Functions |
    `----------*/

    cryptography::Digest
    hash(Identifier const& identifier,
         cryptography::PublicKey const& subject_K,
         elle::String const& description,
         certificate::Permissions const& permissions,
         std::chrono::system_clock::time_point const& valid_from,
         std::chrono::system_clock::time_point const& valid_until)
    {
      ELLE_TRACE_FUNCTION(identifier, subject_K, description,
                          permissions, valid_from, valid_until);

      elle::Natural32 _valid_from =
        static_cast<elle::Natural32>(
          std::chrono::duration_cast<std::chrono::minutes>(
            valid_from.time_since_epoch()).count());
      elle::Natural32 _valid_until =
        static_cast<elle::Natural32>(
          std::chrono::duration_cast<std::chrono::minutes>(
            valid_until.time_since_epoch()).count());

      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  identifier,
                  subject_K,
                  description,
                  permissions,
                  _valid_from,
                  _valid_until),
                cryptography::KeyPair::oneway_algorithm));
    }
  }
}
