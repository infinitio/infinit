#include "User.hh"

#include <boost/lexical_cast.hpp>

#include <elle/log.hh>
#include <elle/serialization/json.hh>

#include <fist/tests/server.hh>

ELLE_LOG_COMPONENT("fist.tests");

namespace tests
{
  User::User(elle::UUID id,
             std::string email,
             boost::optional<infinit::cryptography::rsa::KeyPair> keys,
             std::unique_ptr<papier::Identity> identity)
    : infinit::oracles::meta::User()
    , _email(std::move(email))
    , _keys(keys)
    , _identity(std::move(identity))
    , _facebook_id(boost::lexical_cast<std::string>(elle::UUID::random()))
  {
    this->id = id.repr();
    if (this->_keys)
    {
      std::string public_key_serialized;
      this->_keys.get().K().Save(public_key_serialized);
      this->public_key = public_key_serialized;
      this->register_status = "ok";
    }
    else
    {
      this->register_status = "ghost";
    }
  }

  void
  User::add_connected_device(elle::UUID const& device_id)
  {
    for (auto const& id: this->connected_devices)
      if (device_id == id)
        return;
    this->connected_devices.push_back(device_id);
  }

  void
  User::remove_connected_device(elle::UUID const& device_id)
  {
    auto& vec = this->connected_devices;
    vec.erase(std::remove(vec.begin(), vec.end(), device_id), vec.end());
  }

  void
  User::print(std::ostream& stream) const
  {
    stream << "User(" << this->email() << ", " << this->id << ")";
  }

  std::string
  User::json() const
  {
    std::stringstream ss;
    {
      elle::serialization::json::SerializerOut output(ss, false);
      auto meta_user = static_cast<infinit::oracles::meta::User>(*this);
      meta_user.serialize(output);
    }
    return ss.str();
  }

  Client::Client(Server& server,
                 User const& user,
                 boost::filesystem::path const& home_path)
    : _server(server)
    , device_id(elle::UUID::random())
    , user(const_cast<User&>(user))
    , state(server, device_id, home_path)
  {
    state->attach_callback<surface::gap::State::ConnectionStatus>(
      [&] (surface::gap::State::ConnectionStatus const& notif)
      {
        ELLE_TRACE_SCOPE("connection status: %s %s %s",
                         notif.status ? "online" : "offline",
                         notif.still_trying ? "trying" : "",
                         !notif.last_error.empty() ? notif.last_error : "");
      }
      );

    state->attach_callback<surface::gap::State::UserStatusNotification>(
      [&] (surface::gap::State::UserStatusNotification const& notif)
      {
        ELLE_TRACE_SCOPE("user (%s) status changed: %s",
                         notif.id, notif.status ? "online" : "offline");
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
    this->user.add_connected_device(this->device_id);
  }

  void
  Client::logout()
  {
    this->state->logout();
    this->user.remove_connected_device(this->device_id);
  }
}
