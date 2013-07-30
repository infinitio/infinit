#ifndef PLASMA_PLASMA_HXX
# define PLASMA_PLASMA_HXX

# include <elle/serialize/Serializer.hh>
# include <elle/serialize/NamedValue.hh>

ELLE_SERIALIZE_NO_FORMAT(plasma::Transaction);

#define DEFAULT_FILL_VALUE_RENAME(_ar_, _res_, _name_, _default_, _new_name_)  \
  try                                                                          \
  {                                                                            \
    _ar_ & elle::serialize::named(#_name_, _res_._new_name_);                  \
  }                                                                            \
  catch (...)                                                                  \
  {                                                                            \
    ELLE_WARN((#_name_ " is missing. Using default value %s"), _default_);     \
    _res_._new_name_ = _default_;                                              \
  } /* */

#define DEFAULT_FILL_VALUE(_ar_, _res_, _name_, _default_)                     \
  DEFAULT_FILL_VALUE_RENAME(_ar_, _res_, _name_, _default_, _name_) /* */

ELLE_SERIALIZE_SIMPLE(plasma::Transaction, ar, res, version)
{
  enforce(version == 0);

  ar & named("_id", res.id);
  ar & named("sender_id", res.sender_id);
  ar & named("sender_fullname", res.sender_fullname);
  ar & named("sender_device_id", res.sender_device_id);
  ar & named("recipient_id", res.recipient_id);
  ar & named("recipient_fullname", res.recipient_fullname);
  ar & named("recipient_device_id", res.recipient_device_id);
  ar & named("recipient_device_name", res.recipient_device_name);
  ar & named("network_id", res.network_id);
  ar & named("message", res.message);
  ar & named("first_filename", res.first_filename);
  ar & named("files_count", res.files_count);
  ar & named("total_size", res.total_size);
  ar & named("timestamp", res.timestamp);
  ar & named("is_directory", res.is_directory);
  ar & named("status", res.status);
}

#endif
