#ifndef USER_HH
# define USER_HH

# include <plasma/meta/Client.hh>

# include <surface/crust/Authority.hh>
# include <surface/crust/MetaAuthority.hh>

# include <infinit/Identity.hh>

# include <elle/attribute.hh>

# include <boost/filesystem/path.hpp>

# include <memory>

class User
{
  ELLE_ATTRIBUTE(std::unique_ptr<infinit::Identity>, identity);
  ELLE_ATTRIBUTE_P(boost::filesystem::path, identity_path, mutable);
  ELLE_ATTRIBUTE_P(boost::filesystem::path, user_path, mutable);

public:
  User(std::string const& passord,
       std::string const& description = "XXX",
       infinit::Authority const& authority = infinit::MetaAuthority{});

  User(boost::filesystem::path const& idendity_path);

  /*------.
  | Local |
  `------*/
public:
  /// Store identity localy.
  void
  store(boost::filesystem::path const& identity_path = common::infinit::home(),
        bool force = false) const;

  /// Delete local descritor.
  void
  erase(boost::filesystem::path const& identity_path = "");

  /// Install.
  void
  install(std::string const& name,
          boost::filesystem::path const& path = common::infinit::home()) const;

  /// Uninstall.
  void
  uninstall(std::string const& name = "",
            boost::filesystem::path const& path = "") const;

  // /*------------.
  // | Publication |
  // `------------*/
public:
  // /// Publish identity to the remote meta.
  // void
  // publish(std::string const& host = common::meta::host(),
  //         uint16_t port = common::meta::port(),
  //         std::string const& token = common::meta::token()) const;

  // /// Remove identity from remote meta.
  // void
  // unpublish(std::string const& host = common::meta::host(),
  //           uint16_t port = common::meta::port(),
  //           std::string const& token = common::meta::token()) const;

#define WRAP_IDENTITY(_type_, _name_)                                          \
  ELLE_ATTRIBUTE_r_ACCESSOR(_type_, _name_)                                    \
  {                                                                            \
    ELLE_ASSERT_NEQ(this->_identity, nullptr);                                 \
    return this->_identity->_name_();                                          \
  }

  WRAP_IDENTITY(elle::String, identifier);
  WRAP_IDENTITY(elle::String, description);

#undef WRAP_IDENTITY
};

#endif
