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
    reporter.start();
    reporter.store("user_landing");
    reporter.store("user_signup");
    reporter.store("user_fullname");
    reporter.store("user_email");
    reporter.store("user_password");
    reporter.store("user_code");

    reporter.store("user_register_attempt");
    reporter.store("user_register_succeed");
    reporter.store("user_register_fail");
    reporter.store("user_login_attempt");
    reporter.store("user_login_succeed");
    reporter.store("user_login_fail");
    reporter.store("user_logout_attempt");
    reporter.store("user_logout_succeed");
    reporter.store("user_logout_fail");

    reporter.store("network_create_attempt");
    reporter.store("network_create_succeed", {{MKey::value,"31231"}});
    reporter.store("network_create_fail");
    reporter.store("network_delete_attempt", {{MKey::value,"31231"}});
    reporter.store("network_delete_succeed", {{MKey::value,"31231"}});
    reporter.store("network_delete_fail", {{MKey::value,"31231"}});
    reporter.store("network_adduser_attempt", {{MKey::value,"31231"}});
    reporter.store("network_adduser_succeed", {{MKey::value,"31231"}});
    reporter.store("network_adduser_fail",  {{MKey::value,"31231"}});
    reporter.store("network_removeuser_attempt", {{MKey::value,"31231"}});
    reporter.store("network_removeuser_succeed", {{MKey::value,"31231"}});
    reporter.store("network_removeuser_fail", {{MKey::value,"31231"}});
    reporter.store("transaction_create_attempt", {{MKey::size,"31231"}, {MKey::count,"31231"}});
    reporter.store("transaction_create_succeed", {{MKey::value,"31231"}, {MKey::size,"31231"}, {MKey::count,"31231"}});
    reporter.store("transaction_create_fail", {{MKey::size,"31231"}, {MKey::count,"31231"}});
    reporter.store("transaction_ready_attempt", {{MKey::value,"31231"}});
    reporter.store("transaction_ready_succeed", {{MKey::value,"31231"}});
    reporter.store("transaction_ready_fail", {{MKey::value,"31231"}});
    reporter.store("transaction_accept_attempt", {{MKey::value,"31231"}});
    reporter.store("transaction_accept_succeed", {{MKey::value,"31231"}});
    reporter.store("transaction_accept_fail", {{MKey::value,"31231"}});
    reporter.store("transaction_start_attempt", {{MKey::value,"31231"}});
    reporter.store("transaction_start_succeed", {{MKey::value,"31231"}});
    reporter.store("transaction_start_fail", {{MKey::value,"31231"}});
    reporter.store("transaction_finish_attempt", {{MKey::value,"31231"}});
    reporter.store("transaction_finish_succeed", {{MKey::value,"31231"}});
    reporter.store("transaction_finish_fail", {{MKey::value,"31231"}});
    reporter.store("transaction_cancel_attempt",{{MKey::author,"sender"}, {MKey::status,"attempt"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel_succeed",{{MKey::author,"sender"}, {MKey::status,"succeed"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel_fail",{{MKey::author,"sender"}, {MKey::status,"fail"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel_attempt",{{MKey::author,"recipient"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel_succeed",{{MKey::author,"recipient"}, {MKey::value,"31231"}});
    reporter.store("transaction_cancel_fail",{{MKey::author,"recipient"}, {MKey::value,"31231"}});
    reporter.store("connect_google_attempt");
    reporter.store("connect_google_succeed");
    reporter.store("connect_google_fail");
    reporter.store("connect_facebook_attempt");
    reporter.store("connect_facebook_succeed");
    reporter.store("connect_facebook_fail");
    reporter.store("login_google_attempt");
    reporter.store("login_google_succeed");
    reporter.store("login_google_fail");
    reporter.store("login_facebook_attempt");
    reporter.store("login_facebook_succeed");
    reporter.store("login_facebook_fail");
    reporter.store("import_google_attempt");
    reporter.store("import_google_succeed");
    reporter.store("import_google_fail");
    reporter.store("import_facebook_attempt");
    reporter.store("import_facebook_succeed");
    reporter.store("import_facebook_fail");
    reporter.store("google_invite_attempt");
    reporter.store("google_invite_succeed");
    reporter.store("google_invite_fail");
    reporter.store("facebook_invite_attempt");
    reporter.store("facebook_invite_succeed");
    reporter.store("facebook_invite_fail");
    reporter.store("google_share_attempt");
    reporter.store("google_share_succeed");
    reporter.store("google_share_fail");
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
    reporter.store("transaction_create_attempt");

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_user", {{MKey::input, "keyboard"}});
    reporter.store("searchbar_share", {{MKey::input, "keyboard"}});
    reporter.store("transaction_create_attempt");

// Share to Social Contact

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_social", {{MKey::input, "click"}});
    reporter.store("searchbar_share", {{MKey::input, "click"}});
    reporter.store("transaction_create_attempt");

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_social", {{MKey::input, "keyboard"}});
    reporter.store("searchbar_share", {{MKey::input, "keyboard"}});
    reporter.store("transaction_create_attempt");

// Share to Email Address

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("searchbar_share", {{MKey::input, "click"}});
    reporter.store("transaction_create_attempt");

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("searchbar_share", {{MKey::input, "keyboard"}});
    reporter.store("transaction_create_attempt");

// Share to Other Contact

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_other", {{MKey::input, "click"}});
    reporter.store("searchbar_share", {{MKey::input, "click"}});
    reporter.store("transaction_create_attempt");

    reporter.store("drop_bar");
    reporter.store("searchbar_search");
    reporter.store("select_other", {{MKey::input, "keyboard"}});
    reporter.store("searchbar_share", {{MKey::input, "keyboard"}});
    reporter.store("transaction_create_attempt");

// Sign Up

    reporter.store("user_landing");
    reporter.store("user_signup");
    reporter.store("user_fullname");
    reporter.store("user_email");
    reporter.store("user_password");
    reporter.store("user_code");
    reporter.store("user_register_succeed");

// Cancel via Notifications

    reporter.store("panel_open", {{MKey::panel, "transfer"}});
    reporter.store("panel_cancel", {{MKey::panel, "transfer"}});
    reporter.store("transaction_cancel_attempt", {{MKey::author, "sender"}});

    reporter.store("panel_open", {{MKey::panel, "notification"}});
    reporter.store("panel_cancel", {{MKey::panel, "notification"}, {MKey::author, "recipient"}});
    reporter.store("transaction_cancel_attempt", {{MKey::author, "recipient"}});

// Core Funnel

    reporter.store("transaction_create_succeed");
    reporter.store("panel_accept", {{MKey::panel, "transfer"}});
    reporter.store("transaction_accept_attempt");
    reporter.store("transaction_accept_succeed");
    reporter.store("transaction_prepare_attempt");
    reporter.store("transaction_prepare_succeed");
    reporter.store("transaction_start_attempt");
    reporter.store("transaction_start_succeed");
    reporter.store("transaction_finish_attempt");
    reporter.store("transaction_finish_succeed");
  }

  elle::printf("Test done.\n");

  return 0;
}
