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

    namespace operations
    {
      Operations const identity = 0;
      Operations const descriptor = (1 << 0);
      Operations const passport = (1 << 1);
      Operations const certificate = (1 << 2);
    }
  }

  /*-------------.
  | Construction |
  `-------------*/

  Certificate::Certificate(cryptography::PublicKey issuer_K,
                           cryptography::PublicKey subject_K,
                           certificate::Operations operations,
                           std::chrono::system_clock::time_point valid_from,
                           std::chrono::system_clock::time_point valid_until,
                           cryptography::Signature signature):
    _issuer_K(std::move(issuer_K)),
    _subject_K(std::move(subject_K)),
    _operations(std::move(operations)),
    _valid_from(std::move(valid_from)),
    _valid_until(std::move(valid_until)),
    _signature(std::move(signature))
  {
  }

  Certificate::Certificate(Certificate const& other):
    elle::serialize::DynamicFormat<Certificate>(other),
    _issuer_K(other._issuer_K),
    _subject_K(other._subject_K),
    _operations(other._operations),
    _valid_from(other._valid_from),
    _valid_until(other._valid_until),
    _signature(other._signature)
  {
  }

  Certificate::Certificate(Certificate&& other):
    elle::serialize::DynamicFormat<Certificate>(std::move(other)),
    _issuer_K(std::move(other._issuer_K)),
    _subject_K(std::move(other._subject_K)),
    _operations(std::move(other._operations)),
    _valid_from(std::move(other._valid_from)),
    _valid_until(std::move(other._valid_until)),
    _signature(std::move(other._signature))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Certificate,
                                  _issuer_K, _subject_K, _signature)
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
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Certificate::print(std::ostream& stream) const
  {
    stream << "("
           << this->_subject_K << ", "
           << this->_operations
           << ")";
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
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Operations const operations)
    {
      elle::Boolean first = true;

      stream << "(";

      if (operations & operations::identity)
      {
        if (!first)
          stream << " | ";
        stream << "identity";
        first = false;
      }

      if (operations & operations::descriptor)
      {
        if (!first)
          stream << " | ";
        stream << "descriptor";
        first = false;
      }

      if (operations & operations::passport)
      {
        if (!first)
          stream << " | ";
        stream << "passport";
        first = false;
      }

      if (operations & operations::certificate)
      {
        if (!first)
          stream << " | ";
        stream << "certificate";
        first = false;
      }

      stream << ")";

      return (stream);
    }

    /*----------.
    | Functions |
    `----------*/

    cryptography::Digest
    hash(cryptography::PublicKey const& issuer_K,
         cryptography::PublicKey const& subject_K,
         certificate::Operations const& operations,
         std::chrono::system_clock::time_point const& valid_from,
         std::chrono::system_clock::time_point const& valid_until)
    {
      elle::Natural64 _valid_from =
        static_cast<elle::Natural64>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
            valid_from.time_since_epoch()).count());
      elle::Natural64 _valid_until =
        static_cast<elle::Natural64>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
            valid_until.time_since_epoch()).count());

      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  issuer_K,
                  subject_K,
                  operations,
                  _valid_from,
                  _valid_until),
                cryptography::KeyPair::oneway_algorithm));
    }
  }
}
