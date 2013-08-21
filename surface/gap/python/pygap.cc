#include <wrappers/boost/python.hh>

#include <surface/gap/gap.h>
#include <surface/gap/gap_bridge.hh>

#include <surface/gap/State.hh>

static boost::python::str
_gap_user_directory(gap_State* state)
{
  assert(state != nullptr);
  boost::python::str o;
  char const* udir;
  if (gap_user_directory(state, &udir) == nullptr)
  {
    return boost::python::str();
  }
  boost::python::str ret{std::string{udir}};
  free((void *)udir);
  return ret;
}

static boost::python::str
_gap_token(gap_State* state)
{
  assert(state != nullptr);
  boost::python::str o;
  char *tok;
  if (gap_token(state, &tok) == gap_error)
  {
    return boost::python::str();
  }
  boost::python::str ret{std::string{tok}};
  free(tok);
  return ret;
}

static boost::python::str
_gap_generation_key(gap_State* state)
{
  assert(state != nullptr);
  boost::python::str o;
  char *tok = nullptr;
  if (gap_generation_key(state, &tok) == gap_error)
  {
    return boost::python::str();
  }
  boost::python::str ret{std::string{tok}};
  free(tok);
  return ret;
}

static boost::python::object
_get_transactions(gap_State* state)
{
  assert(state != nullptr);

  boost::python::list transactions_;
  uint32_t* transactions = gap_transactions(state);
  uint32_t* todel = transactions;
  if (transactions != nullptr)
  {
    while (*transactions != 0)
    {
      transactions_.append(boost::python::object(*(transactions++)));
    }
    gap_transactions_free(todel);
  }
  return transactions_;
}

static boost::python::object
_get_swaggers(gap_State* state)
{
  assert(state != nullptr);

  boost::python::list swaggers_;
  uint32_t* swaggers = gap_swaggers(state);
  uint32_t* todel = swaggers;
  if (swaggers != nullptr)
  {
    while (*swaggers != 0)
    {
      swaggers_.append(boost::python::object(*(swaggers++)));
    }
    gap_swaggers_free(todel);
  }

  return swaggers_;
}

static boost::python::object
_search_users(gap_State* state, std::string text)
{
  assert(state != nullptr);
  assert(text.length() > 0);

  boost::python::list users_;
  uint32_t* users = gap_search_users(state, text.c_str());
  uint32_t* todel = users;
  if (users != nullptr)
  {
    while (*users != 0)
    {
      users_.append(boost::python::object(*(users++)));
    }
    gap_search_users_free(todel);
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
uint32_t
_send_files_by_email(gap_State* state,
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

  auto id = gap_send_files_by_email(state,
                                    recipient.c_str(),
                                    list);

  free(list);

  return id;
}

static
uint32_t
_send_files(gap_State* state,
            uint32_t peer_id,
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

  auto id = gap_send_files(state, peer_id, list);

  free(list);

  return id;
}

namespace
{
  namespace detail {
    static
    std::string
    parse_python_exception()
    {
      namespace py = boost::python;

      PyObject* type_ptr = NULL;
      PyObject* value_ptr = NULL;
      PyObject* traceback_ptr = NULL;

      PyErr_Fetch(&type_ptr, &value_ptr, &traceback_ptr);

      std::string ret("Unfetchable Python error");

      if(type_ptr != NULL)
      {
        py::handle<> h_type(type_ptr);
        py::str type_pstr(h_type);
        py::extract<std::string> e_type_pstr(type_pstr);
        if(e_type_pstr.check())
          ret = e_type_pstr();
        else
          ret = "Unknown exception type";
      }

      if(value_ptr != NULL)
      {
        py::handle<> h_val(value_ptr);
        py::str a(h_val);
        py::extract<std::string> returned(a);
        if(returned.check())
          ret +=  ": " + returned();
        else
          ret += std::string(": Unparsable Python error: ");
      }

      if(traceback_ptr != NULL)
      {
        py::handle<> h_tb(traceback_ptr);
        py::object tb(py::import("traceback"));
        py::object fmt_tb(tb.attr("format_tb"));
        py::object tb_list(fmt_tb(h_tb));
        py::object tb_str(py::str("\n").join(tb_list));
        py::extract<std::string> returned(tb_str);
        if(returned.check())
          ret += ": " + returned();
        else
          ret += std::string(": Unparsable Python traceback");
      }
      return ret;
    }

    template <typename T>
    struct wrapper
    {
      T const& _callback;
      wrapper(T const &callback)
        : _callback(callback)
      {}

      template <class ...ARGS>
      void
      operator() (ARGS &&... args)
      {
        this->call(std::forward<ARGS>(args)...);
      }

      template <class ...ARGS>
      void
      call(ARGS&&... args)
      {
        try
        {
          _callback(std::forward<ARGS>(args)...);
        }
        catch (boost::python::error_already_set const&)
        {
          std::string msg = parse_python_exception();
          throw surface::gap::Exception{
            gap_api_error,
            elle::sprintf("python: %s", msg),
          };
        }
      }

    };
  } /* detail */

  template <typename T>
  detail::wrapper<T>
  wrap_call(T const& callback)
  {
    return detail::wrapper<T>(callback);
  }

  void
  _gap_new_swagger_callback(gap_State* state,
                            boost::python::object cb)
  {
    auto cpp_cb = [cb] (surface::gap::State::NewSwaggerNotification const& notif)
      {
        wrap_call(cb)(notif.id);
      };

    run<int>(
      state,
      "new swagger callback",
      [&] (surface::gap::State& state) -> int
      {
        state.attach_callback<surface::gap::State::NewSwaggerNotification>(cpp_cb);
        return 0;
      });
  }

  void
  _gap_user_status_callback(gap_State* state,
                           boost::python::object cb)
  {
    auto cpp_cb = [cb] (surface::gap::State::UserStatusNotification const& notif)
      {
        wrap_call(cb)(notif.id, (gap_UserStatus) notif.status);
      };

    run<int>(
      state,
      "user status callback",
      [&] (surface::gap::State& state)
      {
        state.attach_callback<surface::gap::State::UserStatusNotification>(cpp_cb);
        return 0;
      });
  }

  void
  _gap_transaction_callback(gap_State* state,
                            boost::python::object cb)
  {
    auto cpp_cb = [cb] (surface::gap::TransferMachine::Notification const& notif)
    {
      wrap_call(cb)(notif.id, notif.status);
    };

    run<int>(
      state,
      "transaction callback",
      [&] (surface::gap::State& state) -> int
      {
        state.attach_callback<surface::gap::TransferMachine::Notification>(cpp_cb);
        return 0;
      });

  }

  void
  _gap_message_callback(gap_State* state,
                        boost::python::object cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (MessageNotification const& notif) {
        wrap_call(cb)(notif.sender_id.c_str(), notif.message.c_str());
    };

    // reinterpret_cast<surface::gap::State*>(state)->notification_manager().message_callback(cpp_cb);
  }

  void
  _gap_on_error_callback(gap_State* state,
                         boost::python::object cb)
  {
    auto cpp_cb = [cb] (gap_Status status,
                        std::string const& str,
                        std::string const& tid)
    {
      wrap_call(cb)(status, str.c_str(), tid.c_str());
    };

    // reinterpret_cast<surface::gap::State*>(state)->notification_manager().on_error_callback(cpp_cb);
  }
}

extern "C"
{
  //struct gap_State { /* dummy declaration for boost::python */ };
  PyObject* PyInit__gap(); // Pacify -Wmissing-declarations.
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
# include <oracle/disciples/meta/src/meta/error_code.hh.inc>
# undef _TS
# undef ERR_CODE
  ;

  py::enum_<TransferState>("TransactionStatus")
    .value("NewTransaction", TransferState_NewTransaction)
    .value("SenderCreateNetwork", TransferState_SenderCreateNetwork)
    .value("SenderCreateTransaction", TransferState_SenderCreateTransaction)
    .value("SenderCopyFiles", TransferState_SenderCopyFiles)
    .value("SenderWaitForDecision", TransferState_SenderWaitForDecision)
    .value("RecipientWaitForDecision", TransferState_RecipientWaitForDecision)
    .value("RecipientAccepted", TransferState_RecipientAccepted)
    .value("GrantPermissions", TransferState_GrantPermissions)
    .value("PublishInterfaces", TransferState_PublishInterfaces)
    .value("Connect", TransferState_Connect)
    .value("PeerDisconnected", TransferState_PeerDisconnected)
    .value("PeerConnectionLost", TransferState_PeerConnectionLost)
    .value("Transfer", TransferState_Transfer)
    .value("CleanLocal", TransferState_CleanLocal)
    .value("CleanRemote", TransferState_CleanRemote)
    .value("Finished", TransferState_Finished)
    .value("Rejected", TransferState_Rejected)
    .value("Canceled", TransferState_Canceled)
    .value("Failed", TransferState_Failed)
  ;

  //- gap ctor and dtor -------------------------------------------------------

  py::class_<gap_State, boost::noncopyable>("State");

  py::def("new",
          &gap_new,
          py::return_value_policy<py::return_opaque_pointer>());
  py::def("configurable_new",
          &gap_configurable_new,
          py::return_value_policy<py::return_opaque_pointer>());
  py::def("free", &gap_free);

  py::def("enable_debug", &gap_enable_debug);

  //- Authentication and registration -----------------------------------------

  py::def("hash_password", &_hash_password, by_value());
  py::def("login", &gap_login);
  py::def("token", &_gap_token);
  py::def("generation_key", &_gap_generation_key);
  py::def("is_logged", &gap_logged_in);
  py::def("logout", &gap_logout);
  py::def("register", &gap_register);
  py::def("user_directory", *_gap_user_directory);

  //- Notifications ------------------------------------------------------------

  py::def("pull_notifications", &gap_pull_notifications);
  py::def("notifications_read", &gap_notifications_read);
  py::def("poll", &gap_poll);

  ///////////////////////////
  // Callbacks.

  py::def("transaction_callback", &_gap_transaction_callback);
  py::def("message_callback", &_gap_message_callback);
  py::def("on_error_callback", &_gap_on_error_callback);
  py::def("new_swagger_callback", &_gap_new_swagger_callback);
  py::def("user_status_callback", &_gap_user_status_callback);

   //- Infinit services status -------------------------------------------------

  py::def("meta_status", &gap_meta_status);

  //- Device ------------------------------------------------------------------

  py::def("device_status", &gap_device_status);
  py::def("set_device_name", &gap_set_device_name);

  //- Users -------------------------------------------------------------------

  py::def("user_fullname", &gap_user_fullname, by_value());
  py::def("user_handle", &gap_user_handle, by_value());
  py::def("_id", &gap_self_id, by_value());
  py::def("email", &gap_self_email, by_value());
  py::def("remaining_invitations", &gap_self_remaining_invitations);
  py::def("search_users", &_search_users);
  py::def("get_swaggers", &_get_swaggers);

  //- Permissions -------------------------------------------------------------

  py::enum_<gap_Permission>("Permission")
    .value("gap_read", gap_read)
    .value("gap_write", gap_write)
    .value("gap_exec", gap_exec)
    .export_values()
  ;

  //- Transactions- ------------------------------------------------------------

  py::def("transactions", &_get_transactions);
  py::def("send_files", &_send_files, by_value());
  py::def("send_files_by_email", &_send_files_by_email, by_value());
  py::def("cancel_transaction", &gap_cancel_transaction);
  py::def("accept_transaction", &gap_accept_transaction);
  py::def("reject_transaction", &gap_reject_transaction);
  py::def("join_transaction", &gap_join_transaction);
  py::def("set_output_dir", &gap_set_output_dir);
  py::def("get_output_dir", &gap_get_output_dir);
  py::def("transaction_progress", &gap_transaction_progress);
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
}
