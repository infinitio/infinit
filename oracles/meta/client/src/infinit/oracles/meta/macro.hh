#ifndef INFINIT_ORACLES_META_MACRO_HH
# define INFINIT_ORACLES_META_MACRO_HH

// - API responses serializers ------------------------------------------------
#define SERIALIZE_RESPONSE(type, archive, value)                              \
  ELLE_SERIALIZE_NO_FORMAT(type);                                             \
  ELLE_SERIALIZE_SIMPLE(type, archive, value, version)                        \
  {                                                                           \
    enforce(version == 0);                                                    \
    archive & named("success", value._success);                               \
    if (!value.success())                                                     \
    {                                                                         \
      int* n = (int*) &value.error_code;                                      \
      archive & named("error_code", *n);                                      \
      archive & named("error_details", value.error_details);                  \
      return;                                                                 \
    }                                                                         \
    ResponseSerializer<type>::serialize(archive, value);                      \
  }                                                                           \
  template<> template<typename Archive, typename Value>                       \
  void elle::serialize::ResponseSerializer<type>::serialize(Archive& archive, \
                                                            Value& value)     \
/**/

namespace elle
{
  namespace serialize
  {
    template<typename T>
    struct ResponseSerializer
    {
      ELLE_SERIALIZE_BASE_CLASS_MIXIN_TN(T, 0)

      template<typename Archive, typename Value>
      static void serialize(Archive&, Value&);
    };
  }
}

#endif
