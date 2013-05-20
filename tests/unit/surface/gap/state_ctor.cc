// For std::this_thread::sleep_for until gcc4.8
#define _GLIBCXX_USE_NANOSLEEP 1

#include <surface/gap/State.hh>

#include <lune/Lune.hh>
#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <elle/print.hh>

#include <thread>
#include <chrono>
#include <ctime>

ELLE_LOG_COMPONENT("test.State");

using State = surface::gap::State;
using TransactionNotification =
  ::plasma::trophonius::TransactionNotification;
using TransactionStatusNotification =
  ::plasma::trophonius::TransactionStatusNotification;

int fail_counter = 0;

void
auto_accept_transaction_cb(TransactionNotification const &tn, State &s)
{
  s.transaction_manager().update(tn.transaction.id,
                                 gap_transaction_status_accepted);
}

void
close_on_finished_transaction_cb(TransactionStatusNotification const &tn,
                                 State &,
                                 bool& finish_test)
{
  if (tn.status == gap_transaction_status_canceled)
    {
      ELLE_ERR("transaction canceled")
        ++fail_counter;
      finish_test = true;
    }
  else if (tn.status == gap_transaction_status_finished)
    finish_test = true;
}

auto make_login = []
(State &s, std::string user, std::string email)
{
  try
  {
    s.register_(user,
                email,
                s.hash_password(email, "bitebitebite"),
                "bitebite");
  }
  catch (...)
  {
    s.login(email,
            s.hash_password(email, "bitebitebite"));
  }
  s.update_device("device" + user);
};

void
work(surface::gap::State& state)
{
  while (state.logged_in())
  {
    state.notification_manager().poll();
    ::sleep(1);
  }
}

auto make_worker = []
(State &s) -> std::thread
{
  return std::move(std::thread{[&] { work(s); }});
};

void
error_cb(gap_Status s, std::string const& msg, std::string const& tid)
{
  // Ugly, but easier to see the error.
  std::cerr << "==========================================" << std::endl;
  std::cerr << "(" << s << "): " << msg << std::endl;
  std::cerr << "==========================================" << std::endl;
}

std::string email1 = elle::os::getenv("INFINIT_SENDER", "_sender01@infinit.io");
std::string email2 = elle::os::getenv("INFINIT_RECIEVER", "_rec01@infinit.io");

std::tuple<surface::gap::State*, std::thread*>
init_sender(std::string const& to_send, unsigned int count = 10)
{
  static surface::gap::State state;
  int timeout = 0;
  bool finish = false;
  unsigned int counter = 0;

  make_login(state, "Bite", email1);
  state.notification_manager().transaction_status_callback([&] (TransactionStatusNotification const& t,
                                                                bool)
                                                           {
                                                             close_on_finished_transaction_cb(
                                                               t, state, finish);
                                                           });

  state.notification_manager().on_error_callback(error_cb);

  static std::thread thread = make_worker(state);

  try
  {
    for (counter = 0; counter < count; ++counter)
    {
      ELLE_LOG("%s send / %s", counter, count);
      auto operation_id = state.transaction_manager().send_files(email2, {to_send});

      auto operation_status = surface::gap::OperationManager::OperationStatus::running;

      timeout = 30;
      while (operation_status == surface::gap::OperationManager::OperationStatus::running)
      {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (--timeout < 0)
          throw std::runtime_error{"sending files timed out"};

        operation_status = state.transaction_manager().status(operation_id);
      }
      state.transaction_manager().finalize(operation_id);

      timeout = 60;
      while (!finish)
      {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (--timeout < 0)
          throw std::runtime_error{"downloading files timed out"};
      }

      finish = false;
    }
  }
  catch (...)
  {
    ELLE_ERR("sending failed");
  }

  return std::make_tuple(&state, &thread);
}

std::tuple<surface::gap::State*, std::thread*>
init_recipient()
{
  static surface::gap::State state;

  make_login(state, "Bite", email2);
  state.notification_manager().transaction_callback([&] (TransactionNotification const& t, bool)
                                                    { auto_accept_transaction_cb(t, state); });

  state.notification_manager().on_error_callback(error_cb);

  static std::thread thread = make_worker(state);

  return std::make_tuple(&state, &thread);
}

int
main(int argc, char** argv)
{
  lune::Lune::Initialize();

  if (argc == 1) // No args
  {
    std::string to_send{"to_send"};

    auto const& rstate = init_recipient();
    auto const& sstate = init_sender(to_send);

    std::get<0>(rstate)->logout();
    std::get<0>(sstate)->logout();

    std::get<1>(rstate)->join();
    std::get<1>(sstate)->join();
  }
  else if (argc == 2 && std::string{argv[1]} == "--from")
  {
    auto const& rstate = init_recipient();

    std::get<0>(rstate)->notification_manager().user_status_callback(
      [&rstate] (surface::gap::UserStatusNotification const& notif)
      {
        auto id = std::get<0>(rstate)->user_manager().one(email1).id;

        if (notif.user_id == id && notif.status == 0)
        {
          std::this_thread::sleep_for(std::chrono::seconds(3));
          std::get<0>(rstate)->logout();
        }
      });

    // Hack!!! Will poll 2 times faster but keep the reciepent alive.
    work(*(std::get<0>(rstate)));
    std::get<1>(rstate)->join();
  }
  else if (argc == 2)
  {
    auto const& rstate = init_recipient();
    auto const& sstate = init_sender(argv[1]);

    std::get<0>(rstate)->logout();
    std::get<0>(sstate)->logout();

    std::get<1>(rstate)->join();
    std::get<1>(sstate)->join();
  }
  else if (argc == 3 && std::string{argv[1]} == "--to")
  {
    auto const& sstate = init_sender(argv[2]);

    std::get<0>(sstate)->logout();
    std::get<1>(sstate)->join();
  }
  else if (argc == 3 &&
           std::string{argv[1]} == "--from" &&
           std::string{argv[2]} == "--4ever")
  {
    auto const& rstate = init_recipient();

    // Hack!!! Will poll 2 times faster but keep the reciepent alive.
    // Will never stop.
    work(*(std::get<0>(rstate)));

    std::get<1>(rstate)->join();

  }
  else if (argc == 3)
  {
    auto const& rstate = init_recipient();
    auto const& sstate = init_sender(std::string{argv[1]}, atoi(argv[2]));

    std::get<0>(rstate)->logout();
    std::get<0>(sstate)->logout();

    std::get<1>(rstate)->join();
    std::get<1>(sstate)->join();
  }
  else if (argc == 4 && std::string{argv[1]} == "--to")
  {
    auto const& sstate = init_sender(std::string{argv[2]}, atoi(argv[3]));

    std::get<0>(sstate)->logout();
    std::get<1>(sstate)->join();
  }
  else
  {
    ELLE_ERR("BAD COMMAND LINE, ask Antony");
    std::cerr << "FAIL" << std::endl;
  }


  std::cerr << fail_counter << std::endl;
  elle::print("tests done.");
  return 0;
}
