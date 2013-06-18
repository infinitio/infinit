#ifndef INFINIT_CERTIFICATE_HXX
# define INFINIT_CERTIFICATE_HXX

# include <elle/serialize/VectorSerializer.hxx>

namespace infinit
{
  /*-------------.
  | Construction |
  `-------------*/

  template <typename T>
  Certificate::Certificate(cryptography::PublicKey issuer_K,
                           cryptography::PublicKey subject_K,
                           certificate::Operations operations,
                           std::chrono::system_clock::time_point valid_from,
                           std::chrono::system_clock::time_point valid_until,
                           T const& authority):
    Certificate(std::move(issuer_K),
                std::move(subject_K),
                std::move(operations),
                std::move(valid_from),
                std::move(valid_until),
                authority.sign(
                  certificate::hash(issuer_K,
                                    subject_K,
                                    operations,
                                    valid_from,
                                    valid_until)))
  {
  }
}

/*-------------.
| Serializable |
`-------------*/

ELLE_SERIALIZE_SPLIT(infinit::Certificate);

ELLE_SERIALIZE_SPLIT_SAVE(infinit::Certificate,
                          archive,
                          value,
                          format)
{
  enforce(format == 0, "unknown format");

  archive << value._issuer_K;
  archive << value._subject_K;
  archive << value._operations;

  elle::Natural64 valid_from =
    static_cast<elle::Natural64>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
        value._valid_from.time_since_epoch()).count());
  archive << valid_from;

  elle::Natural64 valid_until =
    static_cast<elle::Natural64>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
        value._valid_until.time_since_epoch()).count());
  archive << valid_until;

  archive << value._signature;
}

ELLE_SERIALIZE_SPLIT_LOAD(infinit::Certificate,
                          archive,
                          value,
                          format)
{
  enforce(format == 0, "unknown format");

  archive >> value._issuer_K;
  archive >> value._subject_K;
  archive >> value._operations;

  elle::Natural64 valid_from;
  archive >> valid_from;
  std::chrono::nanoseconds _valid_from(valid_from);
  value._valid_from += _valid_from;

  elle::Natural64 valid_until;
  archive >> valid_until;
  std::chrono::nanoseconds _valid_until(valid_until);
  value._valid_until += _valid_until;

  archive >> value._signature;
}

#endif
