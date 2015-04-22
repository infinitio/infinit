#include <elle/log.hh>
#include <elle/test.hh>

#include <infinit/oracles/trophonius/Client.hh>
#include <surface/gap/State.hh>
#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.tests");

class Trophonius
  : public tests::Trophonius
{
public:
  Trophonius()
    : socket(nullptr)
  {}

  virtual
  void
  _serve(std::unique_ptr<reactor::network::SSLSocket> socket) override
  {
    static bool first = true;
    if (first)
    {
      first = false;
      this->socket = socket.get();
      tests::Trophonius::_serve(std::move(socket));
    }
    else
    {
      while (true)
      {
        auto json_read = elle::json::read(*socket);
        auto json = boost::any_cast<elle::json::Object>(json_read);
        if (json.find("poke") != json.end())
        {
          auto poke = json["poke"];
          elle::json::write(*socket, json);
        }
        else
        {
          ELLE_LOG("send invalid credentials notification");
          using infinit::oracles::trophonius::NotificationType;
          elle::fprintf(
            *socket, "{\"notification_type\": %s}\n",
            static_cast<int>(NotificationType::invalid_credentials));
          socket->flush();
        }
      }
    }
  }

  reactor::network::SSLSocket* socket;
};

ELLE_TEST_SCHEDULED(trophonius_reconnection)
{
  auto trophonius = elle::make_unique<Trophonius>();
  auto& tropho = *trophonius;
  tests::Server server(std::move(trophonius));
  auto const& user = server.register_user("sender@infinit.io", "password");
  tests::Client client(server, user);
  client.login();
  BOOST_CHECK(tropho.socket);
  tropho.socket->close();
  reactor::wait(!client.state->logged_in());
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 15;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(trophonius_reconnection), 0, timeout);
}
