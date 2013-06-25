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
                           elle::String description,
                           certificate::Permissions permissions,
                           std::chrono::system_clock::time_point valid_from,
                           std::chrono::system_clock::time_point valid_until,
                           T const& issuer,
                           Identifier identifier):
    Certificate(std::move(issuer_K),
                std::move(subject_K),
                std::move(description),
                std::move(permissions),
                std::move(valid_from),
                std::move(valid_until),
                issuer.sign(
                  certificate::hash(identifier,
                                    subject_K,
                                    description,
                                    permissions,
                                    valid_from,
                                    valid_until)),
                std::move(identifier))
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
  archive << value._identifier;
  archive << value._subject_K;
  archive << value._description;
  archive << value._permissions;

  elle::Natural32 valid_from =
    static_cast<elle::Natural32>(
      std::chrono::duration_cast<std::chrono::minutes>(
        value._valid_from.time_since_epoch()).count());
  archive << valid_from;

  elle::Natural32 valid_until =
    static_cast<elle::Natural32>(
      std::chrono::duration_cast<std::chrono::minutes>(
        value._valid_until.time_since_epoch()).count());
  archive << valid_until;

  archive << value._issuer_signature;
}

ELLE_SERIALIZE_SPLIT_LOAD(infinit::Certificate,
                          archive,
                          value,
                          format)
{
  enforce(format == 0, "unknown format");

  archive >> value._issuer_K;
  archive >> value._identifier;
  archive >> value._subject_K;
  archive >> value._description;
  archive >> value._permissions;

  elle::Natural32 valid_from;
  archive >> valid_from;
  std::chrono::minutes _valid_from(valid_from);
  value._valid_from += _valid_from;

  elle::Natural32 valid_until;
  archive >> valid_until;
  std::chrono::minutes _valid_until(valid_until);
  value._valid_until += _valid_until;

  archive >> value._issuer_signature;
}

#endif
