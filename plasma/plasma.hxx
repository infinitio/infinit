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
  ELLE_LOG_COMPONENT("infinit.plasma");
  enforce(version == 0);

  try
  {
    ar & elle::serialize::named("_id", res.id);
  }
  catch (...)
  {
    ar & elle::serialize::named("transaction_id", res.id);
  }

  ar & elle::serialize::named("sender_id", res.sender_id);
  ar & elle::serialize::named("sender_fullname", res.sender_fullname);
  ar & elle::serialize::named("sender_device_id", res.sender_device_id);
  ar & elle::serialize::named("recipient_id", res.recipient_id);
  ar & elle::serialize::named("recipient_fullname", res.recipient_fullname);
  ar & elle::serialize::named("recipient_device_id", res.recipient_device_id);
  ar & elle::serialize::named("recipient_device_name", res.recipient_device_name);
  ar & elle::serialize::named("network_id", res.network_id);
  ar & elle::serialize::named("message", res.message);
  ar & elle::serialize::named("first_filename", res.first_filename);
  ar & elle::serialize::named("files_count", res.files_count);
  ar & elle::serialize::named("total_size", res.total_size);
  ar & elle::serialize::named("status", res.status);

  DEFAULT_FILL_VALUE(ar, res, is_directory, false);
  DEFAULT_FILL_VALUE(ar, res, timestamp, 0.0f);
  DEFAULT_FILL_VALUE(ar, res, early_accepted, false);
}

#endif
