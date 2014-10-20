#ifndef INFINIT_ORACLES_PEER_TRANSACTION_HXX
# define INFINIT_ORACLES_PEER_TRANSACTION_HXX

# include <elle/serialize/Serializer.hh>
# include <elle/serialize/NamedValue.hh>
# include <elle/serialize/ListSerializer.hxx>

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::PeerTransaction);

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

ELLE_SERIALIZE_SIMPLE(infinit::oracles::PeerTransaction, ar, res, version)
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
  ar & named("message", res.message);
  ar & named("files", res.files);
  ar & named("files_count", res.files_count);
  ar & named("total_size", res.total_size);
  ar & named("ctime", res.ctime);
  ar & named("mtime", res.mtime);
  ar & named("is_directory", res.is_directory);
  ar & named("status", res.status);
  try
  { // No way for a proper check
    ar & named("is_ghost", res.is_ghost);
  }
  catch(...)
  {
  }
    try
  { // No way for a proper check
    ar & named("download_link", res.download_link);
  }
  catch(...)
  {
  }
}

namespace std
{
  template<>
  struct hash<infinit::oracles::PeerTransaction>
  {
  public:
    std::size_t operator()(infinit::oracles::PeerTransaction const& tr) const
    {
      return std::hash<std::string>()(tr.id);
    }
  };
}

#endif
