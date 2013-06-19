#include <surface/crust/User.hh>

#include <infinit/Identifier.hh>

#include <common/common.hh>

#include <infinit/Certificate.hh>

#include <cryptography/Cryptosystem.hh>

#include <elle/log.hh>
#include <elle/Exception.hh>
#include <elle/assert.hh>

ELLE_LOG_COMPONENT("suface.crust.User");

User::User(std::string const& description,
           std::string const& passphrase,
           infinit::Authority const& authority)
{
  infinit::Identifier uid(32);
  auto key_pair =
    cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa, 1024);

  this->_identity.reset(
    new infinit::Identity(common::meta::certificate().subject_K(),
                          uid.value(),
                          description,
                          key_pair,
                          passphrase,
                          authority));
}

User::User(boost::filesystem::path const& identity_path)
{
  if (!boost::filesystem::exists(identity_path))
    throw elle::Exception(
      elle::sprintf("file %s doesn't exist", identity_path));

  this->_identity.reset(
    new infinit::Identity(
      elle::serialize::from_file(identity_path.string())));
  this->_identity_path = identity_path;
}

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
  this->_identity_path = identity_path;
}

void
User::erase(boost::filesystem::path const& identity_path)
{
  ELLE_TRACE_METHOD(identity_path);
  ELLE_ASSERT_NEQ(this->_identity, nullptr);

  if (!this->_identity_path.empty() && identity_path.empty())
    boost::filesystem::remove(this->_identity_path);
  else if (!identity_path.empty())
    boost::filesystem::remove(identity_path);
  else
    throw elle::Exception("no path given");
}

void
User::install(std::string const& name,
              boost::filesystem::path const& home_path) const
{
  ELLE_ASSERT_NEQ(this->_identity, nullptr);

  if (!boost::filesystem::exists(home_path))
    throw elle::Exception(
      elle::sprintf("directory %s set as infinit_home doesn't exist",
                    home_path));

  auto const& user_path =
    common::infinit::user_directory(name, home_path.string());

  boost::filesystem::create_directory(user_path);

  this->store(common::infinit::identity_path(name,
                                             home_path.string()));

  this->_user_path = user_path;
}

void
User::uninstall(std::string const& name,
                boost::filesystem::path const& home_path) const
{
  ELLE_ASSERT_NEQ(this->_identity, nullptr);

  if (!boost::filesystem::exists(home_path))
    throw elle::Exception(
      elle::sprintf("directory %s set as infinit_home doesn't exist",
                    home_path));

  auto const& user_path =
    common::infinit::user_directory(name, home_path.string());

  if (!boost::filesystem::exists(user_path))
    throw elle::Exception(
    elle::sprintf("user path %s already exists", user_path));

  if (!this->_user_path.empty() && user_path.empty())
    boost::filesystem::remove(this->_user_path);
  else if (!user_path.empty())
    boost::filesystem::remove_all(user_path);
  else
    throw elle::Exception("no path given");
}

// void
// User::publish(std::string const& host,
//                  uint16_t port,
//                  std::string const& token) const
// {
//   using namespace elle::serialize;

//   std::string serialized;
//   to_string<OutputBase64Archive>(serialized) << *this->_identity;

//   auto id = meta(host, port, token).identity_publish(serialized).id;
// }

// void
// User::unpublish(std::string const& host,
//                    uint16_t port,
//                    std::string const& token) const
// {
//   ELLE_ASSERT_NEQ(this->_identity, nullptr);

//   meta(host, port, token).identity_unpublish(this->identifier());
// }
