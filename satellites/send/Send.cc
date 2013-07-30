#include <iostream>
#include <stdexcept>

#include <boost/program_options.hpp>

#include <elle/Exception.hh>

#include <lune/Lune.hh>
#include <surface/gap/gap.h>
#include <surface/gap/State.hh>
#include <version.hh>

static
void
mandatory(boost::program_options::variables_map const& options,
          std::string const& option)
{
  if (!options.count(option))
    throw elle::Exception(
      elle::sprintf("missing mandatory option: %s", option));
}

static
boost::program_options::variables_map
parse_options(int argc, char** argv)
{
  using namespace boost::program_options;
  options_description options("Allowed options");
  options.add_options()
    ("help,h", "display the help")
    ("user,u", value<std::string>(), "the username")
    ("password,p", value<std::string>(), "the password")
    ("to,t", value<std::string>(), "the recipient")
    ("file,f", value<std::string>(), "the file to send");

  variables_map vm;
  try
  {
    store(parse_command_line(argc, argv, options), vm);
    notify(vm);
  }
  catch (invalid_command_line_syntax const& e)
  {
    throw elle::Exception(elle::sprintf("command line error: %s", e.what()));
  }

  if (vm.count("help"))
  {
    std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << options;
    std::cout << std::endl;
    std::cout << "Infinit " INFINIT_VERSION
      " Copyright (c) 2013 infinit.io All rights reserved." << std::endl;
    exit(0); // XXX: use Exit exception
  }

  mandatory(vm, "user");
  mandatory(vm, "password");
  mandatory(vm, "to");
  mandatory(vm, "file");

  return vm;
}


int main(int argc, char** argv)
{
  try
  {
    auto options = parse_options(argc, argv);
    std::string const user = options["user"].as<std::string>();
    std::string const password = options["password"].as<std::string>();
    std::string const to = options["to"].as<std::string>();
    std::string const file = options["file"].as<std::string>();

    lune::Lune::Initialize(); // XXX

    surface::gap::State state;

    auto hashed_password = state.hash_password(user, password);
    state.login(user, hashed_password);
    // state.update_device("lust");

    auto id = state.send_files(to, { file.c_str() });

    bool stop = false;
    state.notification_manager().transaction_callback(
      [&] (plasma::trophonius::TransactionNotification const& t, bool)
      {
        // if (transaction_id != t.id)
        //   return;

        if (t.status == plasma::TransactionStatus::finished ||
            t.status == plasma::TransactionStatus::canceled)
          stop = true;
      });

    while (!stop)
      state.notification_manager().poll(1);

    state.join_transaction(id);
  }
  catch (std::runtime_error const& e)
  {
    std::cerr << argv[0] << ": " << e.what() << "." << std::endl;
    return 1;
  }
}
