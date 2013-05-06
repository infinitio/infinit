#include <metrics/Reporter.hh>
#include <metrics/_details/google.hh>
#include <metrics/_details/kissmetrics.hh>

#include <common/common.hh>

#include <elle/print.hh>
#include <elle/log.hh>

#include <cstdlib>
#include <iostream>
#include <ctime>

ELLE_LOG_COMPONENT("test.Metrics");

int main(void)
{
  ELLE_LOG("...");

  // Initialization.
  std::srand(std::time(0));

  std::string base;

  std::pair<std::string, std::string> users{
    base + std::to_string(std::rand() % 1000),
      base + std::to_string(std::rand() % 1000),
      };

  int transaction_base = std::rand() % 100000000;

  std::vector<std::string> transactions;

  for (int i = 0; i < 8; ++i)
  {
    transactions.push_back(std::to_string(transaction_base + i));
  }

  {

    using MKey = elle::metrics::Key;

    // // Initialize server.
    // auto const& g_info = common::metrics::google_info();
    // elle::metrics::google::register_service(g_info.server,
    //                                         g_info.port,
    //                                         g_info.id_path);

    // Initialize server.
    elle::metrics::Reporter reporter{};

    elle::metrics::kissmetrics::register_service(reporter);
    reporter.store("user_landing");
    reporter.store("user_signup");
    reporter.store("user_fullname");
    reporter.store("user_email");
    reporter.store("user_password");
    reporter.store("user_code");

    reporter.store("user_register",{{MKey::status,"attempt"}});
    reporter.store("user_register",{{MKey::status,"succeed"}});
    reporter.store("user_register",{{MKey::status,"fail"}});
    reporter.store("user_login",{{MKey::status,"attempt"}});
    reporter.store("user_login",{{MKey::status,"succeed"}});
    reporter.store("user_login",{{MKey::status,"fail"}});
    reporter.store("user_logout",{{MKey::status,"attempt"}});
    reporter.store("user_logout",{{MKey::status,"succeed"}});
    reporter.store("user_logout",{{MKey::status,"fail"}});

    reporter.store("network_create",{{MKey::status,"attempt"}});
    reporter.store("network_create",{{MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("network_create",{{MKey::status,"fail"}});
    reporter.store("network_delete",{{MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("network_delete",{{MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("network_delete",{{MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("network_adduser",{{MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("network_adduser",{{MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("network_adduser",{{MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("network_removeuser",{{MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("network_removeuser",{{MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("network_removeuser",{{MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("transaction_create",{{MKey::status,"attempt"}, {MKey::size,"31231"}, {MKey::count,"31231"}});
    reporter.store("transaction_create",{{MKey::status,"succeed"}, {MKey::value,"31231"}, {MKey::size,"31231"}, {MKey::count,"31231"}});
    reporter.store("transaction_create",{{MKey::status,"fail"}, {MKey::size,"31231"}, {MKey::count,"31231"}});
    reporter.store("transaction_ready",{{MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("transaction_ready",{{MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("transaction_ready",{{MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("transaction_accept",{{MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("transaction_accept",{{MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("transaction_accept",{{MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("transaction_start",{{MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("transaction_start",{{MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("transaction_start",{{MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("transaction_finish",{{MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("transaction_finish",{{MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("transaction_finish",{{MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel",{{MKey::author,"sender"}, {MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel",{{MKey::author,"sender"}, {MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel",{{MKey::author,"sender"}, {MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel",{{MKey::author,"recipient"}, {MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel",{{MKey::author,"recipient"}, {MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel",{{MKey::author,"recipient"}, {MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("connect_google",{{MKey::status,"attempt"}});
    reporter.store("connect_google",{{MKey::status,"succeed"}});
    reporter.store("connect_google",{{MKey::status,"fail"}});
    reporter.store("connect_facebook",{{MKey::status,"attempt"}});
    reporter.store("connect_facebook",{{MKey::status,"succeed"}});
    reporter.store("connect_facebook",{{MKey::status,"fail"}});
    reporter.store("login_google",{{MKey::status,"attempt"}});
    reporter.store("login_google",{{MKey::status,"succeed"}});
    reporter.store("login_google",{{MKey::status,"fail"}});
    reporter.store("login_facebook",{{MKey::status,"attempt"}});
    reporter.store("login_facebook",{{MKey::status,"succeed"}});
    reporter.store("login_facebook",{{MKey::status,"fail"}});
    reporter.store("import_google",{{MKey::status,"attempt"}});
    reporter.store("import_google",{{MKey::status,"succeed"}});
    reporter.store("import_google",{{MKey::status,"fail"}});
    reporter.store("import_facebook",{{MKey::status,"attempt"}});
    reporter.store("import_facebook",{{MKey::status,"succeed"}});
    reporter.store("import_facebook",{{MKey::status,"fail"}});
    reporter.store("google_invite",{{MKey::status,"attempt"}});
    reporter.store("google_invite",{{MKey::status,"succeed"}});
    reporter.store("google_invite",{{MKey::status,"fail"}});
    reporter.store("facebook_invite",{{MKey::status,"attempt"}});
    reporter.store("facebook_invite",{{MKey::status,"succeed"}});
    reporter.store("facebook_invite",{{MKey::status,"fail"}});
    reporter.store("google_share",{{MKey::status,"attempt"}});
    reporter.store("google_share",{{MKey::status,"succeed"}});
    reporter.store("google_share",{{MKey::status,"fail"}});
    reporter.store("drop_self");
    reporter.store("drop_favorite");
    reporter.store("drop_bar");
    reporter.store("drop_user");
    reporter.store("drop_nowhere");
    reporter.store("click_self");
    reporter.store("click_favorite");
    reporter.store("click_searchbar");
    reporter.store("transfer_self");
    reporter.store("transfer_favorite");
    reporter.store("transfer_user");
    reporter.store("transfer_social");
    reporter.store("transfer_email");
    reporter.store("panel_open",{{MKey::panel,"transfer"}});
    reporter.store("panel_open",{{MKey::panel,"notification"}});
    reporter.store("panel_close",{{MKey::panel,"transfer"}});
    reporter.store("panel_close",{{MKey::panel,"notification"}});
    reporter.store("panel_accept",{{MKey::panel,"transfer"}});
    reporter.store("panel_accept",{{MKey::panel,"notification"}});
    reporter.store("panel_deny",{{MKey::panel,"transfer"}});
    reporter.store("panel_deny",{{MKey::panel,"notification"}});
    reporter.store("panel_access",{{MKey::panel,"transfer"}});
    reporter.store("panel_access",{{MKey::panel,"notification"}});
    reporter.store("panel_cancel",{{MKey::panel,"transfer"}, {MKey::author,"sender"}});
    reporter.store("panel_cancel", {{MKey::panel,"notification"}, {MKey::author, "recipient"}});
    reporter.store("dropzone_open");
    reporter.store("dropzone_close");
    reporter.store("dropzone_removeitem");
    reporter.store("dropzone_removeall");
    reporter.store("searchbar_search");
    reporter.store("searchbar_invite",{{MKey::input,"click"}});
    reporter.store("searchbar_invite",{{MKey::input,"keyboard"}});
    reporter.store("searchbar_share",{{MKey::input,"click"}});
    reporter.store("searchbar_share",{{MKey::input,"keyboard"}});
    reporter.store("select_user",{{MKey::input,"click"}});
    reporter.store("select_user",{{MKey::input,"keyboard"}});
    reporter.store("select_social",{{MKey::input,"click"}});
    reporter.store("select_social",{{MKey::input,"keyboard"}});
    reporter.store("select_other",{{MKey::input,"click"}});
    reporter.store("select_other",{{MKey::input,"keyboard"}});
    reporter.store("select_close",{{MKey::input,"click"}});
    reporter.store("select_close",{{MKey::input,"keyboard"}});

// Share to Infinit Contact

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_user", {{MKey::input, "click"}});
    reporter.store("searchbar_share", {{MKey::input, "click"}});
    reporter.store("transaction_create", {{MKey::status, "attempt"}});

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_user", {{MKey::input, "keyboard"}});
    reporter.store("searchbar_share", {{MKey::input, "keyboard"}});
    reporter.store("transaction_create", {{MKey::status, "attempt"}});

// Share to Social Contact

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_social", {{MKey::input, "click"}});
    reporter.store("searchbar_share", {{MKey::input, "click"}});
    reporter.store("transaction_create", {{MKey::status, "attempt"}});

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_social", {{MKey::input, "keyboard"}});
    reporter.store("searchbar_share", {{MKey::input, "keyboard"}});
    reporter.store("transaction_create", {{MKey::status, "attempt"}});

// Share to Email Address

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("searchbar_share", {{MKey::input, "click"}});
    reporter.store("transaction_create", {{MKey::status, "attempt"}});

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("searchbar_share", {{MKey::input, "keyboard"}});
    reporter.store("transaction_create", {{MKey::status, "attempt"}});

// Share to Other Contact

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_other", {{MKey::input, "click"}});
    reporter.store("searchbar_share", {{MKey::input, "click"}});
    reporter.store("transaction_create", {{MKey::status, "attempt"}});

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_other", {{MKey::input, "keyboard"}});
    reporter.store("searchbar_share", {{MKey::input, "keyboard"}});
    reporter.store("transaction_create", {{MKey::status, "attempt"}});

// Sign Up

    reporter.store("user_landing");
    reporter.store("user_signup");
    reporter.store("user_fullname");
    reporter.store("user_email");
    reporter.store("user_password");
    reporter.store("user_code");
    reporter.store("user_register", {{MKey::status, "succeed"}});

// Cancel via Notifications

    reporter.store("panel_open", {{MKey::panel, "transfer"}});
    reporter.store("panel_cancel", {{MKey::panel, "transfer"}});
    reporter.store("transaction_cancel", {{MKey::status, "attempt"}, {MKey::author, "sender"}});

    reporter.store("panel_open", {{MKey::panel, "notification"}});
    reporter.store("panel_cancel", {{MKey::panel, "notification"}, {MKey::author, "recipient"}});
    reporter.store("transaction_cancel", {{MKey::status, "attempt"}, {MKey::author, "recipient"}});

// Core Funnel

    reporter.store("transaction_create", {{MKey::status, "succeed"}});
    reporter.store("panel_accept", {{MKey::panel, "transfer"}});
    reporter.store("transaction_accept", {{MKey::status, "attempt"}});
    reporter.store("transaction_accept", {{MKey::status, "succeed"}});
    reporter.store("transaction_prepare", {{MKey::status, "attempt"}});
    reporter.store("transaction_prepare", {{MKey::status, "succeed"}});
    reporter.store("transaction_start", {{MKey::status, "attempt"}});
    reporter.store("transaction_start", {{MKey::status, "succeed"}});
    reporter.store("transaction_finish", {{MKey::status, "attempt"}});
    reporter.store("transaction_finish", {{MKey::status, "succeed"}});
  }

  elle::printf("Test done.\n");

  return 0;
}
