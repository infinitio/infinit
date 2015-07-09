#include <algorithm>

#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/Exception.hh>
#define private public
#include <surface/gap/State.hh>
#undef private

#include "server.hh"

ELLE_LOG_COMPONENT("infinit.fist.tests.devices");

static
void
_add_device(surface::gap::Device const& device,
            surface::gap::Model& model)
{
  std::stringstream stream;
  {
    typename elle::serialization::json::SerializerOut output(stream, false);
    std::vector<surface::gap::Device> devices;
    devices.push_back(device);
    output.serialize("devices", devices);
  }
  elle::serialization::json::SerializerIn input(stream, false);
  surface::gap::DasModel::Update u(input);
  u.apply(model);
}

static
surface::gap::Device
_add_device(surface::gap::Model& model)
{
  surface::gap::Device d;
  d.id = elle::UUID::random();
  d.name = d.id.repr();
  _add_device(d, model);
  return d;
}

static
std::string
_remove_device_entry(elle::UUID const& uuid)
{
  return elle::sprintf("{\"$remove\":true,\"id\":\"%s\"}", uuid);
}

static
void
_remove_device(elle::UUID const& uuid,
               surface::gap::Model& model)
{
  auto t = elle::sprintf("{\"devices\":[%s]}",
                         _remove_device_entry(uuid));
  std::stringstream stream(t);
  elle::serialization::json::SerializerIn input(stream, false);
  surface::gap::DasModel::Update u(input);
  u.apply(model);
}

static
void
_update_device(elle::UUID const& uuid,
               std::string const& name,
               surface::gap::Model& model)
{
  auto t = elle::sprintf("{\"devices\":[{\"name\":\"%s\",\"id\":\"%s\"}]}",
                         name, uuid);
  std::stringstream stream(t);
  elle::serialization::json::SerializerIn input(stream, false);
  surface::gap::DasModel::Update u(input);
  u.apply(model);
}

ELLE_TEST_SCHEDULED(add_device)
{
  tests::Server server;
  tests::Client sender(server, "sender@infinit.io");
  sender.login();
  static int devices = 5;
  surface::gap::Device d;
  for (auto i = 0; i < devices; ++i)
    _add_device(sender.state->_model);
  BOOST_CHECK_EQUAL(sender.state->devices().size(), 1 + devices);
}

ELLE_TEST_SCHEDULED(remove_device)
{
  tests::Server server;
  tests::Client sender(server, "sender@infinit.io");
  sender.login();
  bool beacon = false;
  sender.state->attach_callback<surface::gap::State::ConnectionStatus>(
    [&] (surface::gap::State::ConnectionStatus const& connection){
      beacon = true;
    });
  surface::gap::Device d;
  for (auto i = 0; i < 10; ++i)
  {
    d.id = elle::UUID::random();
    BOOST_CHECK_EQUAL(sender.state->devices().size(), 1);
    _add_device(d, sender.state->_model);
    BOOST_CHECK_EQUAL(sender.state->devices().size(), 2);
    _remove_device(d.id, sender.state->_model);
    BOOST_CHECK_EQUAL(sender.state->devices().size(), 1);
  }
  ELLE_ASSERT_EQ(beacon, false);
}

ELLE_TEST_SCHEDULED(update_device)
{
  tests::Server server;
  tests::Client sender(server, "sender@infinit.io");
  std::string new_name = "update_device_foobar";
  sender.login();
  std::string device_name = sender.state->device().name;
  _update_device(sender.state->device().id,
                 new_name,
                 sender.state->_model);
  BOOST_CHECK_EQUAL((std::string) sender.state->device().name, new_name);
  _update_device(sender.state->device().id,
                 device_name,
                 sender.state->_model);
  BOOST_CHECK_EQUAL((std::string) sender.state->device().name, device_name);
  _update_device(sender.state->device().id,
                 device_name,
                 sender.state->_model);
  BOOST_CHECK_EQUAL((std::string) sender.state->device().name, device_name);
}

ELLE_TEST_SCHEDULED(update_other_device)
{
  tests::Server server;
  tests::Client sender(server, "sender@infinit.io");
  std::string new_name = "update_device_foobar";
  sender.login();
  auto other_device = _add_device(sender.state->_model);
  std::string device_name = other_device.name;
  std::string main_device_name = (std::string) sender.state->device().name;
  _update_device(other_device.id, new_name, sender.state->_model);
  // Check that the main device is not impacted.
  BOOST_CHECK_EQUAL((std::string) sender.state->device().name, main_device_name);
  bool beacon = false;
  auto check_device = [&] (elle::UUID const& id, std::string const& name)
  {
    for (auto const* device: sender.state->devices())
    {
      if (device->id == id)
      {
        BOOST_CHECK_EQUAL((std::string) device->name, name);
        beacon = true;
      }
    }
  };
  check_device(other_device.id, new_name);
  BOOST_CHECK(beacon);
  beacon = false;
  _update_device(other_device.id, device_name, sender.state->_model);
  check_device(other_device.id, device_name);
  BOOST_CHECK(beacon);
  beacon = false;
  _update_device(other_device.id, device_name, sender.state->_model);
  check_device(other_device.id, device_name);
  BOOST_CHECK(beacon);
}

static
void
_remove_current_device(int other_devices = 0)
{
  tests::Server server;
  tests::Client sender(server, "sender@infinit.io");
  sender.login();
  for (auto i = 0; i < other_devices; ++i)
    _add_device(sender.state->_model);
  BOOST_CHECK_EQUAL(sender.state->devices().size(), 1 + other_devices);
  bool beacon = false;
  sender.state->attach_callback<surface::gap::State::ConnectionStatus>(
    [&] (surface::gap::State::ConnectionStatus const& connection){
      BOOST_CHECK_EQUAL(connection.status, false);
      BOOST_CHECK_EQUAL(connection.still_trying, false);
      beacon = true;
    });
  _remove_device(sender.state->device().id, sender.state->_model);
  sender.state->poll();
  BOOST_CHECK_EQUAL(beacon, true);
}

ELLE_TEST_SCHEDULED(remove_current_device)
{
  _remove_current_device(0);
}

ELLE_TEST_SCHEDULED(remove_current_device_from_list)
{
  _remove_current_device(5);
}

ELLE_TEST_SCHEDULED(synchronize_remove_devices)
{
  tests::Server server;
  tests::Client sender(server, "sender@infinit.io");
  sender.login();
  int const devices = 5;
  for (auto i = 0; i < devices; ++i)
    _add_device(sender.state->_model);
  BOOST_CHECK_EQUAL(sender.state->devices().size(), 1 + devices);
  sender.state->synchronize();
  BOOST_CHECK_EQUAL(sender.state->devices().size(), 1);
  BOOST_CHECK_EQUAL(sender.state->devices()[0]->id, sender.state->device().id);
}

static
void
_change_synchronize_route(tests::Server& server,
                          std::vector<surface::gap::Device>& devices)
{
  std::random_shuffle(devices.begin(), devices.end());
  server.register_route(
    "/user/synchronize",
    reactor::http::Method::GET,
    [&] (tests::Server::Headers const&,
         tests::Server::Cookies const& cookies,
         tests::Server::Parameters const& parameters,
         elle::Buffer const&)
    {
      auto const& device = server.device(cookies);
      if (devices.empty())
      {
        surface::gap::Device d;
        d.id = device.id();
        d.name = device.id().repr();
        devices.push_back(d);
      }

      std::stringstream stream;
      {
        typename elle::serialization::json::SerializerOut output(stream, false);
        output.serialize("devices", devices);
      }
      std::string devices = stream.str();
      // Remove opening and closing bracket.
      auto beg = devices.begin() + 1;
      auto end = beg;
      std::advance(end, devices.size() - 2);
      devices = std::string(beg, end);
      ELLE_LOG("devices: %s", devices);
      return elle::sprintf(
        "{"
        "  \"swaggers\": [],"
        "  \"running_transactions\": [],"
        "  \"final_transactions\": [],"
        "  \"links\": [],"
        "  \"accounts\": [{\"type\": \"email\", \"id\": \"f@ke.email\"}],"
        "  \"account\": {"
        "    \"plan\": \"basic\","
        "    \"custom_domain\": \"\","
        "    \"link_size_quota\": 0,"
        "    \"link_size_used\": 0"
        "  },"
        "%s"
        "}", devices);
    });
}

ELLE_TEST_SCHEDULED(synchronize_add_devices)
{
  tests::Server server;
  std::vector<surface::gap::Device> devices;
  _change_synchronize_route(server, devices);
  tests::Client sender(server, "sender@infinit.io");
  sender.login();
  for (auto i = 0; i < 5; ++i)
    devices.push_back(_add_device(sender.state->_model));
  BOOST_CHECK_EQUAL(devices.size(), 6);
  BOOST_CHECK_EQUAL(sender.state->devices().size(), devices.size());
  sender.state->synchronize();
  BOOST_CHECK_EQUAL(sender.state->devices().size(), devices.size());
  for (int i = 0; i < 5; ++i)
  {
    surface::gap::Device d;
    d.id = elle::UUID::random();
    devices.push_back(d);
  }
  BOOST_CHECK_EQUAL(devices.size(), 11);
  sender.state->synchronize();
  BOOST_CHECK_EQUAL(sender.state->devices().size(), devices.size());
}

ELLE_TEST_SCHEDULED(synchronize_update_devices)
{
  tests::Server server;
  std::vector<surface::gap::Device> devices;
  _change_synchronize_route(server, devices);
  tests::Client sender(server, "sender@infinit.io");
  bool beacon = false;
  sender.state->attach_callback<surface::gap::State::ConnectionStatus>(
    [&] (surface::gap::State::ConnectionStatus const& connection){
      BOOST_CHECK_EQUAL(connection.status, false);
      BOOST_CHECK_EQUAL(connection.still_trying, false);
      beacon = true;
    });
  sender.login();
  for (auto i = 0; i < 5; ++i)
    devices.push_back(_add_device(sender.state->_model));
  std::string name = "impossible_device";
  for (auto i = 0; i < 5; ++i)
  {
    int beacon = 0;
    for (auto const* device: sender.state->devices())
    {
      BOOST_CHECK_NE((std::string) device->name, name);
      ++beacon;
    }
    BOOST_CHECK_EQUAL(devices.size(), 6);
    BOOST_CHECK_EQUAL(beacon, devices.size());
  }
  for (auto& device: devices)
  {
    device.name = name;
  }
  ELLE_ASSERT_EQ(beacon, false);
  sender.state->synchronize();
  ELLE_ASSERT_EQ(beacon, false);
  for (auto i = 0; i < 5; ++i)
  {
    int beacon = 0;
    for (auto const* device: sender.state->devices())
    {
      BOOST_CHECK_EQUAL((std::string) device->name, name);
      ++beacon;
    }
    BOOST_CHECK_EQUAL(devices.size(), 6);
    BOOST_CHECK_EQUAL(beacon, devices.size());
  }
  BOOST_CHECK_EQUAL((std::string) sender.state->device().name, name);
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 20;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(add_device), 0, timeout);
  suite.add(BOOST_TEST_CASE(remove_device), 0, timeout);
  suite.add(BOOST_TEST_CASE(update_device), 0, timeout);
  suite.add(BOOST_TEST_CASE(update_other_device), 0, timeout);
  suite.add(BOOST_TEST_CASE(remove_current_device), 0, timeout);
  suite.add(BOOST_TEST_CASE(remove_current_device_from_list), 0, timeout);
  suite.add(BOOST_TEST_CASE(synchronize_remove_devices), 0, timeout);
  suite.add(BOOST_TEST_CASE(synchronize_add_devices), 0, timeout);
  suite.add(BOOST_TEST_CASE(synchronize_update_devices), 0, timeout);
}
