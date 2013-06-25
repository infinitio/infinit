#ifndef USER_HH
# define USER_HH

# include <plasma/meta/Client.hh>

# include <surface/crust/Authority.hh>
# include <surface/crust/MetaAuthority.hh>

# include <common/common.hh>

# include <infinit/Identity.hh>

# include <elle/attribute.hh>

# include <boost/filesystem/path.hpp>

# include <memory>

class User
{
  ELLE_ATTRIBUTE(std::unique_ptr<infinit::Identity>, identity);

public:
  explicit
  User(std::string const& passphrase,
       std::string const& descrition,
       infinit::Authority const& authority = infinit::MetaAuthority{});

  explicit
  User(boost::filesystem::path const& idendity_path);

  explicit
  User(std::string const& user_name,
       boost::filesystem::path const& home = common::infinit::home());

  /*------.
  | Local |
  `------*/
public:
  /// Store identity localy.
  void
  store(boost::filesystem::path const& identity_path,
        bool force = false) const;

  /// Delete local descritor.
  static
  void
  erase(boost::filesystem::path const& identity_path);

  /// Install.
  void
  install(std::string const& name,
          boost::filesystem::path const& home = common::infinit::home()) const;

  /// Uninstall.
  static
  void
  uninstall(std::string const& name,
            boost::filesystem::path const& home = common::infinit::home());

   /*------------.
   | Publication |
   `------------*/
public:
  void
  signin(std::string const& name,
         std::string const& host = common::meta::host(),
         uint16_t port = common::meta::port()) const;

  static
  void
  signout(std::string const& host = common::meta::host(),
          uint16_t port = common::meta::port(),
          std::string const& token = common::meta::token());


  /// Publish identity to the remote meta.
  void
  publish(std::string const& password,
          std::string const& host = common::meta::host(),
          uint16_t port = common::meta::port(),
          std::string const& token = common::meta::token()) const;

  /// Remove identity from remote meta.
  static
  void
  unpublish(std::string const& host = common::meta::host(),
            uint16_t port = common::meta::port(),
            std::string const& token = common::meta::token());


  /*------.
  | Login |
  `------*/
  std::string
  login(std::string const& password,
        std::string const& host,
        uint16_t port) const;

  static
  std::string
  store_token(std::string const& token,
              std::string const& user_name,
              boost::filesystem::path const& home = common::infinit::home());


#define WRAP_IDENTITY(_type_, _name_)                                          \
  ELLE_ATTRIBUTE_r_ACCESSOR(_type_, _name_)                                    \
  {                                                                            \
    ELLE_ASSERT_NEQ(this->_identity, nullptr);                                 \
    return this->_identity->_name_();                                          \
  }

  WRAP_IDENTITY(infinit::Identifier, identifier);
  WRAP_IDENTITY(elle::String, description);

#undef WRAP_IDENTITY
};

#endif
