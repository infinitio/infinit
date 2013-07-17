#include <metrics/Reporter.hh>
#include <metrics/Service.hh>

#include <common/common.hh>

#include <elle/print.hh>

namespace
{
  class TestService:
    public metrics::Service
  {
  public:
    TestService(std::string const& pkey):
      metrics::Service{pkey, common::metrics::Info{
        "Pretty name", "host addr", 101, "id path", "tracking id"
      }}
    {}

  private:
    void
    _send(metrics::TimeMetricPair) override
    {
    }
    std::string
    _format_event_name(std::string const& name)
    {
      return name;
    }
  };
}

int main(void)
{
  // Initialize server.
  metrics::Reporter reporter{};
  reporter.add_service_class<TestService>();
  reporter.start();
  reporter["test_id"].store("user.landing");
  reporter["test_id"].store("user.signup");
  reporter["test_id"].store("user.fullname");
  reporter["test_id"].store("user.email");
  reporter["test_id"].store("user.password");
  reporter["test_id"].store("user.code");

  reporter["test_id"].store("user.register.attempt");
  reporter["test_id"].store("user.register.succeed");
  reporter["test_id"].store("user.register.fail");
  reporter["test_id"].store("user.login.attempt");
  reporter["test_id"].store("user.login.succeed");
  reporter["test_id"].store("user.login.fail");
  reporter["test_id"].store("user.logout.attempt");
  reporter["test_id"].store("user.logout.succeed");
  reporter["test_id"].store("user.logout.fail");

  reporter["test_id"].store("network.create.attempt");
  reporter["test_id"].store("network.create.succeed", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.create.fail");
  reporter["test_id"].store("network.delete.attempt", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.delete.succeed", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.delete.fail", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.adduser.attempt", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.adduser.succeed", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.adduser.fail",  {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.removeuser.attempt", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.removeuser.succeed", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("network.removeuser.fail", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.create.attempt", {{metrics::Key::size,"31231"}, {metrics::Key::count,"31231"}});
  reporter["test_id"].store("transaction.create.succeed", {{metrics::Key::value,"31231"}, {metrics::Key::size,"31231"}, {metrics::Key::count,"31231"}});
  reporter["test_id"].store("transaction.create.fail", {{metrics::Key::size,"31231"}, {metrics::Key::count,"31231"}});
  reporter["test_id"].store("transaction.ready.attempt", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.ready.succeed", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.ready.fail", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.accept.attempt", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.accept.succeed", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.accept.fail", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.start.attempt", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.start.succeed", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.start.fail", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.finish.attempt", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.finish.succeed", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.finish.fail", {{metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.cancel.attempt",{{metrics::Key::author,"sender"}, {metrics::Key::status,"attempt"}, {metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.cancel.succeed",{{metrics::Key::author,"sender"}, {metrics::Key::status,"succeed"}, {metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.cancel.fail",{{metrics::Key::author,"sender"}, {metrics::Key::status,"fail"}, {metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.cancel.attempt",{{metrics::Key::author,"recipient"}, {metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.cancel.succeed",{{metrics::Key::author,"recipient"}, {metrics::Key::value,"31231"}});
  reporter["test_id"].store("transaction.cancel.fail",{{metrics::Key::author,"recipient"}, {metrics::Key::value,"31231"}});
  reporter["test_id"].store("connect.google.attempt");
  reporter["test_id"].store("connect.google.succeed");
  reporter["test_id"].store("connect.google.fail");
  reporter["test_id"].store("connect.facebook.attempt");
  reporter["test_id"].store("connect.facebook.succeed");
  reporter["test_id"].store("connect.facebook.fail");
  reporter["test_id"].store("login.google.attempt");
  reporter["test_id"].store("login.google.succeed");
  reporter["test_id"].store("login.google.fail");
  reporter["test_id"].store("login.facebook.attempt");
  reporter["test_id"].store("login.facebook.succeed");
  reporter["test_id"].store("login.facebook.fail");
  reporter["test_id"].store("import.google.attempt");
  reporter["test_id"].store("import.google.succeed");
  reporter["test_id"].store("import.google.fail");
  reporter["test_id"].store("import.facebook.attempt");
  reporter["test_id"].store("import.facebook.succeed");
  reporter["test_id"].store("import.facebook.fail");
  reporter["test_id"].store("google.invite.attempt");
  reporter["test_id"].store("google.invite.succeed");
  reporter["test_id"].store("google.invite.fail");
  reporter["test_id"].store("facebook.invite.attempt");
  reporter["test_id"].store("facebook.invite.succeed");
  reporter["test_id"].store("facebook.invite.fail");
  reporter["test_id"].store("google.share.attempt");
  reporter["test_id"].store("google.share.succeed");
  reporter["test_id"].store("google.share.fail");
  reporter["test_id"].store("drop.self");
  reporter["test_id"].store("drop.favorite");
  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("drop.user");
  reporter["test_id"].store("drop.nowhere");
  reporter["test_id"].store("click.self");
  reporter["test_id"].store("click.favorite");
  reporter["test_id"].store("click.searchbar");
  reporter["test_id"].store("transfer.self");
  reporter["test_id"].store("transfer.favorite");
  reporter["test_id"].store("transfer.user");
  reporter["test_id"].store("transfer.social");
  reporter["test_id"].store("transfer.email");
  reporter["test_id"].store("panel.open",{{metrics::Key::panel,"transfer"}});
  reporter["test_id"].store("panel.open",{{metrics::Key::panel,"notification"}});
  reporter["test_id"].store("panel.close",{{metrics::Key::panel,"transfer"}});
  reporter["test_id"].store("panel.close",{{metrics::Key::panel,"notification"}});
  reporter["test_id"].store("panel.accept",{{metrics::Key::panel,"transfer"}});
  reporter["test_id"].store("panel.accept",{{metrics::Key::panel,"notification"}});
  reporter["test_id"].store("panel.deny",{{metrics::Key::panel,"transfer"}});
  reporter["test_id"].store("panel.deny",{{metrics::Key::panel,"notification"}});
  reporter["test_id"].store("panel.access",{{metrics::Key::panel,"transfer"}});
  reporter["test_id"].store("panel.access",{{metrics::Key::panel,"notification"}});
  reporter["test_id"].store("panel.cancel",{{metrics::Key::panel,"transfer"}, {metrics::Key::author,"sender"}});
  reporter["test_id"].store("panel.cancel", {{metrics::Key::panel,"notification"}, {metrics::Key::author, "recipient"}});
  reporter["test_id"].store("dropzone.open");
  reporter["test_id"].store("dropzone.close");
  reporter["test_id"].store("dropzone.removeitem");
  reporter["test_id"].store("dropzone.removeall");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("searchbar.invite",{{metrics::Key::input,"click"}});
  reporter["test_id"].store("searchbar.invite",{{metrics::Key::input,"keyboard"}});
  reporter["test_id"].store("searchbar.share",{{metrics::Key::input,"click"}});
  reporter["test_id"].store("searchbar.share",{{metrics::Key::input,"keyboard"}});
  reporter["test_id"].store("select.user",{{metrics::Key::input,"click"}});
  reporter["test_id"].store("select.user",{{metrics::Key::input,"keyboard"}});
  reporter["test_id"].store("select.social",{{metrics::Key::input,"click"}});
  reporter["test_id"].store("select.social",{{metrics::Key::input,"keyboard"}});
  reporter["test_id"].store("select.other",{{metrics::Key::input,"click"}});
  reporter["test_id"].store("select.other",{{metrics::Key::input,"keyboard"}});
  reporter["test_id"].store("select.close",{{metrics::Key::input,"click"}});
  reporter["test_id"].store("select.close",{{metrics::Key::input,"keyboard"}});

// Share to Infinit Contact

  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("select.user", {{metrics::Key::input, "click"}});
  reporter["test_id"].store("searchbar.share", {{metrics::Key::input, "click"}});
  reporter["test_id"].store("transaction.create.attempt");

  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("select.user", {{metrics::Key::input, "keyboard"}});
  reporter["test_id"].store("searchbar.share", {{metrics::Key::input, "keyboard"}});
  reporter["test_id"].store("transaction.create.attempt");

// Share to Social Contact

  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("select.social", {{metrics::Key::input, "click"}});
  reporter["test_id"].store("searchbar.share", {{metrics::Key::input, "click"}});
  reporter["test_id"].store("transaction.create.attempt");

  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("select.social", {{metrics::Key::input, "keyboard"}});
  reporter["test_id"].store("searchbar.share", {{metrics::Key::input, "keyboard"}});
  reporter["test_id"].store("transaction.create.attempt");

// Share to Email Address

  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("searchbar.share", {{metrics::Key::input, "click"}});
  reporter["test_id"].store("transaction.create.attempt");

  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("searchbar.share", {{metrics::Key::input, "keyboard"}});
  reporter["test_id"].store("transaction.create.attempt");

// Share to Other Contact

  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("select.other", {{metrics::Key::input, "click"}});
  reporter["test_id"].store("searchbar.share", {{metrics::Key::input, "click"}});
  reporter["test_id"].store("transaction.create.attempt");

  reporter["test_id"].store("drop.bar");
  reporter["test_id"].store("searchbar.search");
  reporter["test_id"].store("select.other", {{metrics::Key::input, "keyboard"}});
  reporter["test_id"].store("searchbar.share", {{metrics::Key::input, "keyboard"}});
  reporter["test_id"].store("transaction.create.attempt");

// Sign Up

  reporter["test_id"].store("user.landing");
  reporter["test_id"].store("user.signup");
  reporter["test_id"].store("user.fullname");
  reporter["test_id"].store("user.email");
  reporter["test_id"].store("user.password");
  reporter["test_id"].store("user.code");
  reporter["test_id"].store("user.register.succeed");

// Cancel via Notifications

  reporter["test_id"].store("panel.open", {{metrics::Key::panel, "transfer"}});
  reporter["test_id"].store("panel.cancel", {{metrics::Key::panel, "transfer"}});
  reporter["test_id"].store("transaction.cancel.attempt", {{metrics::Key::author, "sender"}});

  reporter["test_id"].store("panel.open", {{metrics::Key::panel, "notification"}});
  reporter["test_id"].store("panel.cancel", {{metrics::Key::panel, "notification"}, {metrics::Key::author, "recipient"}});
  reporter["test_id"].store("transaction.cancel.attempt", {{metrics::Key::author, "recipient"}});

// Core Funnel

  reporter["test_id"].store("transaction.create.succeed");
  reporter["test_id"].store("panel.accept", {{metrics::Key::panel, "transfer"}});
  reporter["test_id"].store("transaction.accept.attempt");
  reporter["test_id"].store("transaction.accept.succeed");
  reporter["test_id"].store("transaction.prepare.attempt");
  reporter["test_id"].store("transaction.prepare.succeed");
  reporter["test_id"].store("transaction.start.attempt");
  reporter["test_id"].store("transaction.start.succeed");
  reporter["test_id"].store("transaction.finish.attempt");
  reporter["test_id"].store("transaction.finish.succeed");

  elle::print("Test done.");

  return 0;
}
