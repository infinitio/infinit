#include "User.hh"

#include <boost/lexical_cast.hpp>

#include <elle/log.hh>
#include <elle/serialization/json.hh>

#include <fist/tests/server.hh>
#include <fist/tests/_detail/uuids.hh>

ELLE_LOG_COMPONENT("fist.tests");

namespace tests
{
  User::User(boost::uuids::uuid id,
             std::string email,
             boost::optional<cryptography::KeyPair> keys,
             std::unique_ptr<papier::Identity> identity)
    : _id(std::move(id))
    , _email(std::move(email))
    , _keys(keys)
    , _identity(std::move(identity))
    , _facebook_id(boost::lexical_cast<std::string>(random_uuid()))
  {}

  std::string
  User::links_json() const
  {
    // I wish I could use elle::serialization::SerializerOut.
    std::string str = "[";
    for (auto const& link: this->links)
      str += link_representation(link.second) + ", ";

    if (!this->links.empty())
      str = str.substr(0, str.length() - 2);
    str += "]";
    return str;
  }

  std::string
  User::swaggers_json() const
  {
    std::string str = "[";
    for (auto const& user: this->swaggers)
      str += user->json() + ", ";

    if (!this->swaggers.empty())
      str = str.substr(0, str.length() - 2);
    str += "]";
    return str;
  }

  void
  User::print(std::ostream& stream) const
  {
    stream << "User(" << this->email() << ", " << this->id() << ")";
  }

  std::string
  User::devices_json() const
  {
    std::string str = "[";
    for (auto const& device: this->connected_devices)
      str += elle::sprintf("\"%s\", ", device);
    if (!this->connected_devices.empty())
      str = str.substr(0, str.length() - 2);
    str += "]";
    return str;
  }

  std::string
  User::json() const
  {
    std::string public_key_serialized;
    if (this->_keys)
      this->_keys.get().K().Save(public_key_serialized);

    auto res = elle::sprintf(
      "{"
      "  \"id\": \"%s\","
      "  \"public_key\": \"%s\","
      "  \"fullname\": \"%s\","
      "  \"handle\": \"%s\","
      "  \"connected_devices\": %s,"
      "  \"status\": %s,"
      "  \"register_status\": \"%s\","
      "  \"_id\": \"%s\""
      "}",
      this->id(),
      public_key_serialized,
      this->email(),
      this->email(),
      this->devices_json(),
      this->connected_devices.empty() ? "false" : "true",
      !this->ghost() ? "ok" : "ghost",
      this->id());
    return res;
  }

  bool
  User::ghost() const
  {
    return !this->_keys;
  }

  std::string
  User::self_json() const
  {
    std::string identity_serialized;
    this->_identity->Save(identity_serialized);

    std::string public_key_serialized;
    this->_keys.get().K().Save(public_key_serialized);
    return elle::sprintf(
      "{"
      "  \"id\": \"%s\","
      "  \"public_key\": \"%s\","
      "  \"fullname\": \"\","
      "  \"handle\": \"\","
      "  \"connected_devices\": %s,"
      "  \"register_status\": \"\","
      "  \"email\": \"%s\","
      "  \"identity\": \"%s\","
      "  \"devices\": [],"
      "  \"favorites\": [],"
      "  \"success\": true"
      "}",
      this->id(),
      public_key_serialized,
      this->devices_json(),
      this->email(),
      identity_serialized);
  }

  Client::Client(Server& server,
                 User const& user,
                 boost::filesystem::path const& home_path)
    : _server(server)
    , device_id(random_uuid())
    , user(const_cast<User&>(user))
    , state(server, device_id, home_path)
  {
    state->attach_callback<surface::gap::State::ConnectionStatus>(
      [&] (surface::gap::State::ConnectionStatus const& notif)
      {
        ELLE_TRACE_SCOPE("connection status notification: %s", notif);
      }
      );

    state->attach_callback<surface::gap::State::UserStatusNotification>(
      [&] (surface::gap::State::UserStatusNotification const& notif)
      {
        ELLE_TRACE_SCOPE("user status notification: %s", notif);
      });
  }

  Client::~Client()
  {
    this->logout();
  }

  Client::Client(Server& server,
                 std::string const& email,
                 boost::filesystem::path const& home_path)
    : Client(server,
             server.register_user(email, "password"),
             home_path)
  {
  }

  void
  Client::login(std::string const& password)
  {
    this->state->login(this->user.email(), password);
    this->state->logged_in().wait();
    this->user.connected_devices.insert(this->device_id);
  }

  void
  Client::logout()
  {
    this->state->logout();
    this->user.connected_devices.erase(this->device_id);
  }

  std::string
  User::link_representation(infinit::oracles::LinkTransaction const& link)
  {
    std::stringstream link_stream;
    {
      typename elle::serialization::json::SerializerOut output(link_stream);
      const_cast<infinit::oracles::LinkTransaction&>(link).serialize(output);
    }
    return link_stream.str();
  }
}
