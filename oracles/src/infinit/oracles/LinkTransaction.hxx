#ifndef INFINIT_ORACLES_LINK_TRANSACTION_HXX
# define INFINIT_ORACLES_LINK_TRANSACTION_HXX

# include <elle/serialize/Serializer.hh>
# include <elle/serialize/NamedValue.hh>
# include <elle/serialize/ListSerializer.hxx>

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::LinkTransaction);

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

ELLE_SERIALIZE_SIMPLE(infinit::oracles::LinkTransaction, ar, res, version)
{
  enforce(version == 0);

  ar & named("id", res.id);
  ar & named("click_count", res.click_count);
  ar & named("ctime", res.ctime);
  ar & named("message", res.message);
  ar & named("mtime", res.mtime);
  ar & named("name", res.name);
  ar & named("sender_device_id", res.sender_device_id);
  ar & named("sender_id", res.sender_id);
  ar & named("share_link", res.share_link);
  ar & named("status", res.status);
}

namespace std
{
  template<>
  struct hash<infinit::oracles::LinkTransaction>
  {
  public:
    std::size_t operator()(infinit::oracles::LinkTransaction const& tr) const
    {
      return std::hash<std::string>()(tr.id);
    }
  };
}

#endif
