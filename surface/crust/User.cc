#include <surface/crust/User.hh>

#include <infinit/Identifier.hh>

#include <common/common.hh>

#include <infinit/Certificate.hh>

#include <cryptography/Cryptosystem.hh>

#include <elle/log.hh>
#include <elle/Exception.hh>
#include <elle/assert.hh>
#include <elle/serialize/insert.hh>

ELLE_LOG_COMPONENT("suface.crust.User");

/*-----.
| Meta |
`-----*/
static
plasma::meta::Client const&
meta(std::string const& host,
     uint16_t port,
     boost::filesystem::path const& tokenpath = "")
{
  static std::string last_host = host;
  static uint16_t last_port = port;
  static boost::filesystem::path last_tokenpath = tokenpath;
  static std::unique_ptr<plasma::meta::Client> client;

  if (!client ||
      last_host != host ||
      last_port != port ||
      last_tokenpath != tokenpath)
  {
    ELLE_TRACE("creating a new meta: %s %s %s", host, port, tokenpath);
    client.reset(new plasma::meta::Client(host, port, true, tokenpath));
  }

  ELLE_ASSERT_NEQ(client, nullptr);
  return *client;
}

/*-----.
| User |
`-----*/
User::User(std::string const& passphrase,
           std::string const& description,
           infinit::Authority const& authority)
{
  ELLE_TRACE_METHOD(passphrase);

  infinit::Identifier uid(32);
  auto key_pair =
    infinit::cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa, 1024);

  this->_identity.reset(
    new infinit::Identity(common::meta::certificate().subject_K(),
                          key_pair.K(),
                          description,
                          key_pair.k(),
                          passphrase,
                          authority,
                          uid));
}

User::User(boost::filesystem::path const& identity_path)
{
  ELLE_TRACE_METHOD(identity_path);
  if (!boost::filesystem::exists(identity_path))
    throw elle::Exception(
      elle::sprintf("file %s doesn't exist", identity_path));

  this->_identity.reset(
    new infinit::Identity(
      elle::serialize::from_file(identity_path.string())));
}

User::User(std::string const& user_name,
           boost::filesystem::path const& home_path):
  User(boost::filesystem::path{
      common::infinit::identity_path(user_name,
                                     home_path.string())})
{}

void
User::store(boost::filesystem::path const& identity_path,
               bool force) const
{
  ELLE_TRACE_METHOD(identity_path);
  ELLE_ASSERT_NEQ(this->_identity, nullptr);

  if (identity_path.empty())
    throw elle::Exception("identity path is empty");

  if (!force && boost::filesystem::exists(identity_path))
    throw elle::Exception(
      elle::sprintf("file %s already exists", identity_path));

  elle::serialize::to_file(identity_path.string()) << *this->_identity;
}

void
User::erase(boost::filesystem::path const& identity_path)
{
  ELLE_TRACE_FUNCTION(identity_path);

  if (identity_path.empty())
    throw elle::Exception("identity path doesn't exist");

  // Check if the given path represents an identity.
  // XXX: Does this code works (optimisation).
  User{identity_path};

  boost::filesystem::remove(identity_path);
}

void
User::install(std::string const& name,
              boost::filesystem::path const& home_path) const
{
  ELLE_TRACE_METHOD(name, home_path);
  ELLE_ASSERT_NEQ(this->_identity, nullptr);

  // Here, the home is not required.
  auto const& user_path =
    common::infinit::user_directory(name, home_path.string());

  boost::filesystem::create_directories(user_path);

  this->store(common::infinit::identity_path(name,
                                             home_path.string()));
}

void
User::uninstall(std::string const& name,
                boost::filesystem::path const& home_path)
{
  ELLE_TRACE_FUNCTION(name, home_path);

  if (!boost::filesystem::exists(home_path))
    throw elle::Exception(
      elle::sprintf("directory %s set as infinit_home doesn't exist",
                    home_path));

  auto const& user_path =
    common::infinit::user_directory(name, home_path.string());

  if (!boost::filesystem::exists(user_path))
    throw elle::Exception(
      elle::sprintf("user path %s doesn't exist", user_path));

  // Check if the given path represents an identity.
  // XXX: Does this code works (optimisation).
  User{boost::filesystem::path(
         common::infinit::identity_path(name,
                                        home_path.string()))};

  boost::filesystem::remove_all(user_path);
}

void
User::signin(std::string const& name,
             std::string const& host,
             uint16_t port) const
{
  ELLE_TRACE_METHOD(name, host, port);
  ELLE_ASSERT_NEQ(this->_identity, nullptr);

  using namespace elle::serialize;

  std::string serialized;
  to_string<OutputBase64Archive>(serialized) << this->_identity->subject_K();

  meta(host, port).signin(serialized, name);
}

void
User::signout(std::string const& host,
              uint16_t port,
              std::string const& token)
{
  meta(host, port, token).signout();
}

std::string
User::login(std::string const& password,
            std::string const& host,
            uint16_t port) const
{
  ELLE_ASSERT_NEQ(this->_identity, nullptr);

  return meta(host, port).challenge_login(
    this->_identity->decrypt_0(password)).token_generation_key;
}

std::string
User::store_token(std::string const& token,
                  std::string const& user_name,
                  boost::filesystem::path const& home)
{
  std::string token_path =
    common::infinit::tokpass_path(user_name, home.string());
  elle::serialize::to_file(token_path) << token;

  return token_path;
}
