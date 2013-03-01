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

    // Initialize server.
    auto const& g_info = common::metrics::google_info();
    elle::metrics::google::register_service(g_info.server,
                                            g_info.port,
                                            g_info.id_path);

    // Initialize server.
    auto const& km_info = common::metrics::km_info();
    elle::metrics::kissmetrics::register_service(km_info.server,
                                                 km_info.port,
                                                 km_info.id_path);

    auto& reporter = elle::metrics::reporter();


    reporter.store("test:transaction:cancel:sender:succeed");
// //  Login success.
//     reporter.store("test:user:login:attempt");
//     reporter.store("test:user:login:succeed");
//     reporter.update_user(users.first);

//     reporter.store("test:ux:drop:bar");
//     reporter.store("test:ux:keyboard:bar:search");
//     reporter.store("test:ux:click:panel:search:select:user");
//     reporter.store("test:ux:click:bar:search:share");
//     reporter.store("test:transaction:create:attempt",
//                    {{MKey::value, transactions[3]},
//                     {MKey::size, "32402"},
//                     {MKey::count, "3"}});
//     reporter.store("test:transaction:create:succeed",
//                  {{MKey::value, transactions[3]}, {MKey::size, "32402"}, {MKey::count, "3"}});
//     reporter.store("test:ux:click:panel:transfer:accept");
//     reporter.store("test:transaction:accept:attempt",  MKey::value, transactions[3]);
//     reporter.store("test:transaction:accept:succeed",  MKey::value, transactions[3]);
//     reporter.store("test:transaction:prepare:attempt", MKey::value, transactions[3]);
//     reporter.store("test:transaction:prepare:succeed", MKey::value, transactions[3]);
//     reporter.store("test:transaction:start:attempt",   MKey::value, transactions[3]);
//     reporter.store("test:transaction:start:succeed",   MKey::value, transactions[3]);
//     reporter.store("test:transaction:finish:attempt",  MKey::value, transactions[3]);
//     reporter.store("test:transaction:finish:succeed",  MKey::value, transactions[3]);



// // Register failure.
//     reporter.store("test:user:register:attempt");
//     reporter.store("test:user:register:fail", MKey::value, "email already taken.");
// // Register success.
//     reporter.store("test:user:register:attempt");
//     reporter.store("test:user:register:succeed");

// // Login failure
//     reporter.store("test:user:login:attempt");
//     reporter.store("test:user:login:fail", MKey::value, "login:password invalid.");

// // Login success.
//     reporter.store("test:user:login:attempt");
//     reporter.store("test:user:login:succeed");
//     reporter.update_user(users.first);

// // Logout failure.
//     reporter.update_user(users.first);
//     reporter.store("test:user:logout:attempt");
//     reporter.store("test:user:logout:fail", MKey::value, "unknown error");

// //  Login success.
//     reporter.store("test:user:login:attempt");
//     reporter.store("test:user:login:succeed");
//     reporter.update_user(users.first);


// // Send file process.
//     reporter.store("test:transaction:create:attempt",
//                  {{MKey::value, transactions[3]},{MKey::size, "32402"}, {MKey::count, "3"}});
//     reporter.store("test:transaction:create:succeed",
//                  {{MKey::value, transactions[3]}, {MKey::size, "32402"}, {MKey::count, "3"}});
//     reporter.store("test:transaction:accept:attempt",  MKey::value, transactions[3]);
//     reporter.store("test:transaction:accept:succeed",  MKey::value, transactions[3]);
//     reporter.store("test:transaction:prepare:attempt", MKey::value, transactions[3]);
//     reporter.store("test:transaction:prepare:succeed", MKey::value, transactions[3]);
//     reporter.store("test:transaction:start:attempt",   MKey::value, transactions[3]);
//     reporter.store("test:transaction:start:succeed",   MKey::value, transactions[3]);
//     reporter.store("test:transaction:finish:attempt",  MKey::value, transactions[3]);
//     reporter.store("test:transaction:finish:succeed",  MKey::value, transactions[3]);

//     // Self share, cancel while downloading.
//     reporter.store("test:ux:drop:self");
//     reporter.store("test:network:create:attempt");
//     reporter.store("test:network:create:success");
//     reporter.store("test:transaction:create:attempt", {{MKey::value, transactions[4]}, {MKey::size, "32402"}, {MKey::count, "3"}});
//     reporter.store("test:transaction:create:succeed", {{MKey::value, transactions[4]}, {MKey::size, "32402"}, {MKey::count, "3"}});
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:panel:transfer:accept");
//     reporter.store("test:transaction:accept:attempt", MKey::value, transactions[4]);
//     reporter.store("test:transaction:accept:succeed", MKey::value, transactions[4]);
//     reporter.store("test:ux:click:panel:transfer:close:nowhere");
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:panel:transfer:close:button");
//     reporter.store("test:transaction:prepare:attempt", MKey::value, transactions[4]);
//     reporter.store("test:network:user:add:attempt");
//     reporter.store("test:network:user:add:succeed");
//     reporter.store("test:transaction:prepare:succeed", MKey::value, transactions[4]);
//     reporter.store("test:transaction:start:attempt", MKey::value, transactions[4]);
//     reporter.store("test:transaction:start:succeed", MKey::value, transactions[4]);
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:panel:transfer:cancel");
//     reporter.store("test:transaction:cancel:attempt", MKey::value, transactions[4]);
//     reporter.store("test:network:delete:attempt");
//     reporter.store("test:network:delete:succeed");
//     reporter.store("test:transaction:cancel:succeed", MKey::value, transactions[4]);
//     reporter.store("test:ux:click:transfer:close:nowhere");

//     // Self share.
//     reporter.store("test:ux:drop:self");
//     reporter.store("test:network:create:attempt");
//     reporter.store("test:network:create:success");
//     reporter.store("test:transaction:create:attempt", {{MKey::value, transactions[4]}, {MKey::size, "32402"}, {MKey::count, "3"}});
//     reporter.store("test:transaction:create:succeed", {{MKey::value, transactions[4]}, {MKey::size, "32402"}, {MKey::count, "3"}});
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:panel:transfer:accept");
//     reporter.store("test:transaction:accept:attempt", MKey::value, transactions[4]);
//     reporter.store("test:transaction:accept:succeed", MKey::value, transactions[4]);
//     reporter.store("test:ux:click:panel:transfer:close:nowhere");
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:panel:transfer:close:button");
//     reporter.store("test:transaction:prepare:attempt", MKey::value, transactions[4]);
//     reporter.store("test:network:user:add:attempt");
//     reporter.store("test:network:user:add:succeed");
//     reporter.store("test:transaction:prepare:succeed", MKey::value, transactions[4]);
//     reporter.store("test:transaction:start:attempt", MKey::value, transactions[4]);
//     reporter.store("test:transaction:start:succeed", MKey::value, transactions[4]);
//     reporter.store("test:transaction:finish:attempt", MKey::value, transactions[4]);
//     reporter.store("test:transaction:finish:succeed", MKey::value, transactions[4]);
//     reporter.store("test:network:delete:attempt");
//     reporter.store("test:network:delete:succeed");

//     // P10.2:
//     reporter.update_user(users.first);
//     reporter.store("test:ux:drop:self");
//     reporter.store("test:network:create");
//     reporter.store("test:transaction:create", {{MKey::value, transactions[5]}, {MKey::size, "32402"}, {MKey::count, "3"}});
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:panel:transfer:accept");
//     reporter.store("test:transaction:accept", MKey::value, transactions[5]);
//     reporter.store("test:ux:click:panel:transfer:close:nowhere");
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:panel:transfer:close:button");
//     reporter.store("test:network:user:add");
//     reporter.store("test:transaction:start", MKey::value, transactions[5]);
//     reporter.store("test:transaction:finish", MKey::value, transactions[5]);
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:panel:transfer:access");
//     reporter.store("test:network:delete");
//     reporter.store("test:ux:click:panel:transfer:close:nowhere");


//     reporter.store("test:ux:drop:favorite");
//     reporter.store("test:ux:drop:bar");
//     reporter.store("test:ux:drop:nowhere");
//     reporter.store("test:ux:click:self");
//     reporter.store("test:ux:click:favorite");
//     reporter.store("test:ux:click:transfer:self");
//     reporter.store("test:ux:click:transfer:favorite");
//     reporter.store("test:ux:click:transfer:user");
//     reporter.store("test:ux:click:transfer:social");
//     reporter.store("test:ux:click:transfer:email");
//     reporter.store("test:ux:click:nowhere");
//     reporter.store("test:ux:click:panel:transfer:open");
//     reporter.store("test:ux:click:panel:transfer:close:button");
//     reporter.store("test:ux:click:panel:transfer:close:nowhere");
//     reporter.store("test:ux:click:panel:transfer:nowhere");
//     reporter.store("test:ux:click:panel:transfer:accept");
//     reporter.store("test:ux:click:panel:transfer:deny");
//     reporter.store("test:ux:click:panel:transfer:cancel");
//     reporter.store("test:ux:click:panel:transfer:access");
//     reporter.store("test:ux:click:bar:search:field");
//     reporter.store("test:ux:click:bar:search:dropzone:empty");
//     reporter.store("test:ux:click:bar:search:dropzone:open");
//     reporter.store("test:ux:click:bar:search:dropzone:close:nowhere");
//     reporter.store("test:ux:click:bar:search:dropzone:close:button");
//     reporter.store("test:ux:click:bar:search:dropzone:nowhere");
//     reporter.store("test:ux:click:bar:search:dropzone:remove:item");
//     reporter.store("test:ux:click:bar:search:dropzone:remove:all");
//     reporter.store("test:ux:keyboard:bar:search");
//     reporter.store("test:ux:click:bar:search:invite");
//     reporter.store("test:ux:click:bar:search:share");
//     reporter.store("test:ux:keyboard:bar:search:invite");
//     reporter.store("test:ux:keyboard:bar:search:share");
//     reporter.store("test:ux:click:panel:search:open");
//     reporter.store("test:ux:click:panel:search:close:nowhere");
//     reporter.store("test:ux:keyboard:panel:search:close:empty");
//     reporter.store("test:ux:click:panel:search:select:user");
//     reporter.store("test:ux:click:panel:search:select:social");
//     reporter.store("test:ux:click:panel:search:select:other");
//     reporter.store("test:ux:keyboard:panel:search:select:user");
//     reporter.store("test:ux:keyboard:panel:search:select:social");
//     reporter.store("test:ux:keyboard:panel:search:select:other");
//     reporter.store("test:ux:click:panel:notifications:open");
//     reporter.store("test:ux:click:panel:notifications:close:icon");
//     reporter.store("test:ux:click:panel:notifications:close:nowhere");
//     reporter.store("test:ux:click:panel:notifications:nowhere");
//     reporter.store("test:ux:click:panel:notifications:accept");
//     reporter.store("test:ux:click:panel:notifications:deny");
//     reporter.store("test:ux:click:panel:notifications:cancel");
//     reporter.store("test:ux:click:panel:notifications:access");
  }
  elle::printf("Test done.\n");

  return 0;
}
