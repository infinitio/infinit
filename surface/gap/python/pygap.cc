
#include <wrappers/boost/python.hh>

#include <surface/gap/gap.h>

#include <surface/gap/State.hh>

static boost::python::object
_get_networks(gap_State* state)
{
  assert(state != nullptr);

  boost::python::list networks_;
  char** networks = gap_networks(state);
  if (networks != nullptr)
    {
      for (char** ptr = networks; *ptr != nullptr; ++ptr)
        {
          networks_.append(boost::python::str(std::string(*ptr)));
        }
        gap_networks_free(networks);
    }
  return networks_;
}

static boost::python::object
_get_transactions(gap_State* state)
{
  assert(state != nullptr);

  boost::python::list transactions_;
  char** transactions = gap_transactions(state);
  if (transactions != nullptr)
    {
      for (char** ptr = transactions; *ptr != nullptr; ++ptr)
        {
          transactions_.append(boost::python::str(std::string(*ptr)));
        }
        gap_transactions_free(transactions);
    }
  return transactions_;
}

static boost::python::object
_get_swaggers(gap_State* state)
{
  assert(state != nullptr);

  boost::python::list swaggers_;
  char** swaggers = gap_swaggers(state);
  if (swaggers != nullptr)
    {
      for (char** ptr = swaggers; *ptr != nullptr; ++ptr)
        {
          swaggers_.append(boost::python::str(std::string(*ptr)));
        }
        gap_swaggers_free(swaggers);
    }
  return swaggers_;
}

static boost::python::object
_search_users(gap_State* state, std::string text)
{
  assert(state != nullptr);
  assert(text.length() > 0);

  boost::python::list users_;
  char** users = gap_search_users(state, text.c_str());
  if (users != nullptr)
    {
      for (char** ptr = users; *ptr != nullptr; ++ptr)
        {
          users_.append(boost::python::str(std::string(*ptr)));
        }
        gap_search_users_free(users);
    }
  return users_;
}

static std::string
_hash_password(gap_State* state, std::string email, std::string password)
{
  assert(state != nullptr);

  char* hash = gap_hash_password(state, email.c_str(), password.c_str());

  if (hash == nullptr)
    throw std::runtime_error("Couldn't hash the password");

  std::string res(hash);
  gap_hash_free(hash);
  return res;
}

static
gap_ProcessStatus
_send_files(gap_State* state,
            std::string const& recipient,
            boost::python::list const& files)
{
  boost::python::ssize_t len = boost::python::len(files);
  char const** list = (char const**) calloc(sizeof(char*), (len + 1));

  if (list == nullptr)
    throw std::bad_alloc();

  for (int i = 0; i < len; ++i)
    {
      list[i] = boost::python::extract<char const*>(files[i]);
    }

  auto res = gap_send_files(state,
                            recipient.c_str(),
                            list);

  free(list);

  return res;
}


namespace
{
  template<typename T>
  struct wrap
  {
    boost::python::object o;
    void operator ()(T const* b)
    {
      this->o(boost::python::ptr(b));
    }
  };

  void
  _gap_user_status_callback(gap_State* state,
                           boost::python::object cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (UserStatusNotification const& notif) {
        cb(notif.user_id.c_str(), (gap_UserStatus) notif.status);
    };

    reinterpret_cast<surface::gap::State*>(state)->user_status_callback(cpp_cb);
  }

  void
  _gap_transaction_callback(gap_State* state,
                           boost::python::object cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (TransactionNotification const& notif, bool is_new) {
        cb(notif.transaction.transaction_id.c_str(), is_new);
    };

    reinterpret_cast<surface::gap::State*>(state)->transaction_callback(cpp_cb);
  }

  void
  _gap_transaction_status_callback(gap_State* state,
                                  boost::python::object cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (TransactionStatusNotification const& notif, bool is_new) {
        cb(notif.transaction_id.c_str(), is_new);
    };

    reinterpret_cast<surface::gap::State*>(state)->transaction_status_callback(cpp_cb);
  }

  void
  _gap_message_callback(gap_State* state,
                        boost::python::object cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (MessageNotification const& notif) {
        cb(notif.sender_id.c_str(), notif.message.c_str());
    };

    reinterpret_cast<surface::gap::State*>(state)->message_callback(cpp_cb);
  }
}

extern "C"
{
  struct gap_State { /* dummy declaration for boost::python */ };
}

BOOST_PYTHON_MODULE(_gap)
{
  namespace py = boost::python;
  typedef py::return_value_policy<py::return_by_value> by_value;

  //////////////////////////
  // value MUST be gap_EnumName_name
  py::enum_<gap_Status>("Status")
    .value("ok", gap_ok)
    .value("error", gap_error)
    .value("network_error", gap_network_error)
    .value("internal_error", gap_internal_error)
    .value("no_device_error", gap_no_device_error)
    .value("wrong_passport", gap_wrong_passport)
    .value("no_file", gap_no_file)
    .value("file_not_found", gap_file_not_found)
    .value("api_error", gap_api_error)
# define _TS(c) #c
# define ERR_CODE(name, value_, comment)         \
    .value(_TS(name), gap_ ## name)
# include <oracle/disciples/meta/error_code.hh.inc>
# undef _TS
# undef ERR_CODE
  ;

  py::enum_<gap_TransactionStatus>("TransactionStatus")
# define _TS(c) #c
# define TRANSACTION_STATUS(name, value_)                                       \
    .value(_TS(name), gap_transaction_status_ ## name)
# include <oracle/disciples/meta/resources/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
# undef _TS
  ;

  //- gap ctor and dtor -------------------------------------------------------

  py::class_<gap_State>("State");

  py::def("new",
          &gap_new,
          py::return_value_policy<py::return_opaque_pointer>());
  py::def("free", &gap_free);

  py::def("enable_debug", &gap_enable_debug);

  //- Authentication and registration -----------------------------------------

  py::def("hash_password", &_hash_password, by_value());
  py::def("login", &gap_login);
  py::def("is_logged", &gap_logged_in);
  py::def("logout", &gap_logout);
  py::def("register", &gap_register);

  py::def("invite_user", &gap_invite_user);
  py::def("send_message", &gap_message);
  py::def("connect", &gap_trophonius_connect);

  //- Notifications ------------------------------------------------------------

  py::def("pull_notifications", &gap_pull_notifications);
  py::def("notifications_read", &gap_notifications_read);
  py::def("poll", &gap_poll);

  ///////////////////////////
  // Callbacks.

  py::def(
    "transaction_callback",
    &_gap_transaction_callback
  );

  py::def(
    "transaction_status_callback",
    &_gap_transaction_status_callback
  );

  py::def(
    "message_callback",
    &_gap_message_callback
  );

  py::def(
    "user_status_callback",
    &_gap_user_status_callback
  );

   //- Infinit services status -------------------------------------------------

  py::def("meta_status", &gap_meta_status);

  //- Device ------------------------------------------------------------------

  py::def("device_status", &gap_device_status);
  py::def("set_device_name", &gap_set_device_name);

  //- Network -----------------------------------------------------------------

  py::def("networks", &_get_networks);
  py::def("create_network", &gap_create_network);
  py::def("network_name", &gap_network_name, by_value());
  py::def("network_mount_point", &gap_network_mount_point, by_value());
  py::def("network_add_user", &gap_network_add_user);

  //- Users -------------------------------------------------------------------

  py::def("user_fullname", &gap_user_fullname, by_value());
  py::def("user_handle", &gap_user_handle, by_value());
  py::def("_id", &gap_self_id, by_value());
  py::def("email", &gap_self_email, by_value());
  py::def("search_users", &_search_users);
  py::def("get_swaggers", &_get_swaggers);

  //- Permissions -------------------------------------------------------------

  py::enum_<gap_Permission>("Permission")
    .value("gap_read", gap_read)
    .value("gap_write", gap_write)
    .value("gap_exec", gap_exec)
    .export_values()
  ;
  py::def("set_permissions", &gap_set_permissions);

  //- Transactions- ------------------------------------------------------------

  py::def("transactions", &_get_transactions);
  py::def("send_files", &_send_files);
  py::def("update_transaction", &gap_update_transaction);
  py::def("set_output_dir", &gap_set_output_dir);
  py::def("get_output_dir", &gap_get_output_dir);
  py::def("transaction_sender_id", &gap_transaction_sender_id);
  py::def("transaction_sender_fullname", &gap_transaction_sender_fullname);
  py::def("transaction_sender_device_id", &gap_transaction_sender_device_id);
  py::def("transaction_recipient_id", &gap_transaction_recipient_id);
  py::def("transaction_recipient_fullname", &gap_transaction_recipient_fullname);
  py::def("transaction_recipient_device_id", &gap_transaction_recipient_device_id);
  py::def("transaction_network_id", &gap_transaction_network_id);
  py::def("transaction_first_filename", &gap_transaction_first_filename);
  py::def("transaction_files_count", &gap_transaction_files_count);
  py::def("transaction_total_size", &gap_transaction_total_size);
  py::def("transaction_is_directory", &gap_transaction_is_directory);
  py::def("transaction_status", &gap_transaction_status);
  py::def("transaction_message", &gap_transaction_message);

  //- Process ------------------------------------------------------------------
  py::def("process_status", &gap_process_status);
  py::enum_<int>("ProcessStatus")
    .value("failure", gap_process_status_failure)
    .value("success", gap_process_status_success)
    .value("running", gap_process_status_running)
  ;
}
