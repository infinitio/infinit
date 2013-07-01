// For std::this_thread::sleep_for until gcc4.8
#define _GLIBCXX_USE_NANOSLEEP 1

#define BOOST_TEST_MODULE heartbeat
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <surface/gap/State.hh>

#include <lune/Lune.hh>
#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <elle/print.hh>
#include <elle/types.hh>

#include <thread>
#include <cassert>
#include <chrono>
#include <ctime>

ELLE_LOG_COMPONENT("test.State");

typedef ::surface::gap::State State;
typedef std::shared_ptr<State> StatePtr;
typedef ::plasma::trophonius::TransactionNotification TransactionNotification;
typedef surface::gap::OperationManager::OperationStatus OperationStatus;

static elle::Status lune_status = lune::Lune::Initialize();

// Create a state from username and email.
// Try to create the user. If it fails, login.
// Return a shared_ptr on the ready state.
void
make_login(StatePtr state,
           std::string user,
           std::string email)
{
  if (lune_status != elle::Status::Ok)
    throw elle::Exception("lune can't be intialize");

  static std::string password = "ZERTYUIOP";
  try
  {
    state->register_(user,
                     email,
                     state->hash_password(email, password),
                     "bitebite");
  }
  catch (...)
  {
    state->login(email, state->hash_password(email, password));
  }

  state->update_device("device" + user);
};

// Polling function.
// Stop polling when the state is logged out.
void
work(StatePtr state,
     bool& finished)
{
  while (finished)
  {
    state->notification_manager().poll(1);
    ::sleep(1);
  }
  ELLE_TRACE("polling finished");
}

// Create a polling worker for a given state.
std::thread
make_worker(StatePtr state,
            bool& finished)
{
  return std::thread{[&, state] { work(state, finished); }};
};

// Initialize sender, creating the polling thread that keep state alive during
// transfer.
// The callback represent the behavior to adopt on transaction_notification.
template <typename Callback>
std::thread
init_sender(StatePtr state,
            std::string const& recipient_email,
            std::string const& to_send,
            bool& finished,
            Callback&& callback)
{
  int timeout = 0;

  state->notification_manager().transaction_callback(std::move(callback));

  std::thread thread{make_worker(state, finished)};

  try
  {
    auto operation_id = state->transaction_manager().send_files(recipient_email,
                                                                {to_send});

    auto operation_status = OperationStatus::running;

    timeout = 30;
    while (operation_status == OperationStatus::running)
    {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (--timeout < 0)
        throw std::runtime_error{"sending files timed out"};

      operation_status = state->transaction_manager().status(operation_id);
    }
    state->transaction_manager().finalize(operation_id);

    timeout = 60;
    while (timeout > 0)
    {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      --timeout;
    }
  }
  catch (...)
  {
  }

  return thread;
}

// Initialize recipient, creating the polling thread that keep state alive
// during transfer.
// The callback represent the behavior to adopt on transaction_notification.
template <typename Callback>
std::thread
init_recipient(StatePtr state,
               bool& finished,
               Callback&& callback)
{
  state->notification_manager().transaction_callback(std::move(callback));

  std::thread thread{make_worker(state, finished)};
  return thread;
}

BOOST_AUTO_TEST_CASE(state_creation)
{
  std::string state_creator_email = "state_creator_email@lol.fr";

  bool finished = false;
  StatePtr recipient_state{new State()};
  make_login(recipient_state, "state_creator", state_creator_email);

  auto recipient_thread = init_recipient(
    recipient_state,
    finished,
    [&,recipient_state] (TransactionNotification const& t, bool)
    {}
  );

  ::sleep(6);

  finished = true;
  recipient_thread.join();

  recipient_state->logout();
}

BOOST_AUTO_TEST_CASE(delayed_accept)
{
  std::string to_send{"to_send"};

  std::string sender_email = "delayed_acceptsender@lol.fr";
  std::string recipient_email = "delayed_acceptrecipient@lol.fr";

  static int success_counter = 0;

  StatePtr recipient_state{new State()};
  make_login(recipient_state, "delayed_acceptrecipient", recipient_email);

  bool recipient_finish = false;
  auto recipient_thread = init_recipient(
    recipient_state,
    recipient_finish,
    [&, recipient_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Recipient] Transaction %s had been canceled", t);
        recipient_finish = true;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Recipient] Transaction %s failed", t);
        recipient_finish = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("[Recipient] Transaction %s finished", t);
        ++success_counter;
        recipient_finish = true;
      }
      else if (t.status == plasma::TransactionStatus::created && !t.accepted)
      {
        ::sleep(6);
        ELLE_LOG("accepting %s", t);
        recipient_state->transaction_manager().accept_transaction(t.id);
      }
    });

  StatePtr sender_state{new State()};
  make_login(sender_state, "delayed_acceptsender", sender_email);
  bool sender_finished = false;
  auto sender_thread = init_sender(
    sender_state,
    recipient_email,
    to_send,
    sender_finished,
    [&, sender_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Sender] Transaction %s had been canceled", t);
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Sender] Transaction %s failed", t);
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("[Sender] Transaction %s finished", t);
        ++success_counter;
        sender_finished = true;
      }
    });

  recipient_thread.join();
  sender_thread.join();

  BOOST_CHECK_EQUAL(success_counter, 2);
}

BOOST_AUTO_TEST_CASE(early_accept)
{
  std::string to_send{"to_send"};

  std::string sender_email = "early_acceptsender@lol.fr";
  std::string recipient_email = "early_acceptrecipient@lol.fr";

  static int success_counter = 0;

  StatePtr recipient_state{new State()};
  make_login(recipient_state, "early_acceptrecipient", recipient_email);
  bool recipient_finished = false;
  auto recipient_thread = init_recipient(
    recipient_state,
    recipient_finished,
    [&, recipient_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Recipient] Transaction %s had been canceled", t);
        recipient_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Recipient] Transaction %s failed", t);
        recipient_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("[Recipient] Transaction %s finished", t);
        ++success_counter;
        recipient_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::created && !t.accepted)
      {
        ELLE_LOG("accepting %s", t);
        recipient_state->transaction_manager().accept_transaction(t.id);
      }
    });

  StatePtr sender_state{new State()};
  make_login(sender_state, "early_acceptsender", sender_email);
  bool sender_finished = false;
  auto sender_thread = init_sender(
    sender_state,
    recipient_email,
    to_send,
    sender_finished,
    [&, sender_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Sender] Transaction %s had been canceled", t);
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Sender] Transaction %s failed", t);
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("[Sender] Transaction %s finished", t);
        ++success_counter;
        sender_finished = true;
      }
    });

  recipient_thread.join();
  sender_thread.join();

  BOOST_CHECK_EQUAL(success_counter, 2);
}

BOOST_AUTO_TEST_CASE(delayed_cancel)
{
  std::string to_send{"to_send"};

  std::string sender_email = "delayed_cancelsender@lol.fr";
  std::string recipient_email = "delayed_cancelrecipient@lol.fr";

  static int cancel_counter = 0;

  StatePtr recipient_state{new State()};
  make_login(recipient_state, "delayed_cancelrecipient",
                                    recipient_email);

  bool recipient_finished = false;
  auto recipient_thread = init_recipient(
    recipient_state,
    recipient_finished,
    [&, recipient_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Recipient] Transaction %s had been canceled", t);
        recipient_finished = true;
        ++cancel_counter;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Recipient] Transaction %s failed", t);
        recipient_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("[Recipient] Transaction %s finished", t);
        recipient_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::created && !t.accepted)
      {
        ::sleep(6);
        ELLE_LOG("canceling %s", t);
        recipient_state->transaction_manager().cancel_transaction(t.id);
      }
    });

  StatePtr sender_state{new State()};
  make_login(sender_state, "delayed_cancelsender", sender_email);
  bool sender_finished = false;
  auto sender_thread = init_sender(
    sender_state,
    recipient_email,
    to_send,
    sender_finished,
    [&, sender_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Sender] Transaction %s had been canceled", t);
        ++cancel_counter;
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Sender] Transaction %s failed", t);
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("[Sender] Transaction %s finished", t);
        sender_finished = true;
      }
    });

  recipient_thread.join();
  sender_thread.join();

  BOOST_CHECK_EQUAL(cancel_counter, 2);
}

BOOST_AUTO_TEST_CASE(early_cancel)
{
  std::string to_send{"to_send"};

  std::string sender_email = "early_cancelsender@lol.fr";
  std::string recipient_email = "early_cancelrecipient@lol.fr";

  static int cancel_counter = 0;

  StatePtr recipient_state{new State()};
  make_login(recipient_state, "early_cancelrecipient", recipient_email);
  bool recipient_finished = false;
  auto recipient_thread = init_recipient(
    recipient_state,
    recipient_finished,
    [&, recipient_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Recipient] Transaction %s had been canceled", t);
        recipient_finished = true;
        ++cancel_counter;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Recipient] Transaction %s failed", t);
        recipient_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("Transaction %s finished", t);
        recipient_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::created && !t.accepted)
      {
        ::sleep(6);
        ELLE_LOG("canceling %s", t);
        recipient_state->transaction_manager().cancel_transaction(t.id);
      }
    });

  StatePtr sender_state{new State()};
  make_login(sender_state, "cancelsender", sender_email);

  bool sender_finished = false;
  auto sender_thread = init_sender(
    sender_state,
    recipient_email,
    to_send,
    sender_finished,
    [&, sender_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Sender] Transaction %s had been canceled", t);
        ++cancel_counter;
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Sender] Transaction %s failed", t);
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("Transaction %s finished", t);
        sender_finished = true;
      }
    });

  recipient_thread.join();
  sender_thread.join();

  BOOST_CHECK_EQUAL(cancel_counter, 2);
}

BOOST_AUTO_TEST_CASE(ghost_user)
{
  std::string to_send{"to_send"};

  srand(time(NULL));
  auto random_string = [] (size_t size = 24) -> std::string
  {
    static std::string charset = "1234567890abcdef";
    std::string result;
    result.resize(size);

    srand(time(NULL));
    for (size_t i = 0; i < size; i++)
        result[i] = charset[rand() % charset.length()];

    return result;
  };

  std::string sender_email = random_string() + "@lol.fr";
  std::string recipient_email = random_string() + "@lol.fr";

  static int success_counter = 0;

  std::thread recipient_thread;

  StatePtr recipient_state{new State()};
  StatePtr sender_state{new State()};
  bool sender_finished = false;
  make_login(sender_state, "delayed_acceptsender", sender_email);
  auto sender_thread = init_sender(
    sender_state,
    recipient_email,
    to_send,
    sender_finished,
    [&, sender_state, recipient_state] (TransactionNotification const& t, bool)
    {
      if (t.status == plasma::TransactionStatus::canceled)
      {
        ELLE_WARN("[Sender] Transaction %s had been canceled", t);
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::failed)
      {
        ELLE_ERR("[Sender] Transaction %s failed", t);
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::finished)
      {
        ELLE_LOG("[Sender] Transaction %s finished", t);
        ++success_counter;
        sender_finished = true;
      }
      else if (t.status == plasma::TransactionStatus::created &&
               t.accepted == false)
      {
        make_login(recipient_state, "delayed_acceptrecipient", recipient_email);
        bool recipient_finished = false;
        recipient_thread = init_recipient(
          recipient_state,
          recipient_finished,
          [&, recipient_state] (TransactionNotification const& t, bool)
          {
            if (t.status == plasma::TransactionStatus::canceled)
            {
              ELLE_WARN("[Recipient] Transaction %s had been canceled", t);
              recipient_finished = true;
            }
            else if (t.status == plasma::TransactionStatus::failed)
            {
              ELLE_ERR("[Recipient] Transaction %s failed", t);
              recipient_finished = true;
            }
            else if (t.status == plasma::TransactionStatus::finished)
            {
              ELLE_LOG("[Recipient] Transaction %s finished", t);
              ++success_counter;
              recipient_finished = true;
            }
            else if (t.status == plasma::TransactionStatus::created && !t.accepted)
            {
              ::sleep(6);
              ELLE_LOG("accepting %s", t);
              recipient_state->transaction_manager().accept_transaction(t.id);
            }
          });
      }

    });

  sender_thread.join();
  recipient_thread.join();

  BOOST_CHECK_EQUAL(success_counter, 2);
}
