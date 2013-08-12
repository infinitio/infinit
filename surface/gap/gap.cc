#include "gap.h"
#include "State.hh"

#include <common/common.hh>

#include <lune/Lune.hh>

#include <elle/log.hh>
#include <elle/elle.hh>
#include <elle/finally.hh>
#include <elle/HttpClient.hh>
#include <elle/system/Process.hh>
#include <elle/container/list.hh>
#include <CrashReporter.hh>

#include <reactor/scheduler.hh>

#include <plasma/meta/Client.hh>

#include <boost/filesystem.hpp>
#include <boost/range/join.hpp>

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string.h>
#include <unordered_set>

ELLE_LOG_COMPONENT("infinit.surface.gap");

extern "C"
{

  /// - Utils -----------------------------------------------------------------
  struct gap_State
  {
    ELLE_ATTRIBUTE_X(reactor::Scheduler, scheduler);
    ELLE_ATTRIBUTE_R(reactor::Thread, keep_alive);
    ELLE_ATTRIBUTE_R(std::thread, scheduler_thread);
    ELLE_ATTRIBUTE(std::unique_ptr<surface::gap::State>, state);
    ELLE_ATTRIBUTE_R(std::exception_ptr, exception);
  public:
    // reactor::Scheduler&
    // scheduler() const
    // {
    //   return this->_scheduler;
    // }

    surface::gap::State&
    state()
    {
      return *this->_state;
    }

  public:
    gap_State(char const* meta_host,
              unsigned short meta_port,
              char const* trophonius_host,
              unsigned short trophonius_port,
              char const* apertus_host,
              unsigned short apertus_port):
      _scheduler{},
      _keep_alive{this->_scheduler, "State keep alive",
                  [this]
                  {
                    while (true)
                    {
                      auto& current = *this->_scheduler.current();
                      current.sleep(boost::posix_time::seconds(60));
                    }
                  }},
      _scheduler_thread{
        [&]
        {
          try
          {
            this->_scheduler.run();
          }
          catch (...)
          {
            ELLE_ERR("exception escaped from State scheduler: %s",
                     elle::exception_string());
            this->_exception = std::current_exception();
          }
        }}
    {
      this->_scheduler.mt_run<void>(
        "creating state",
        [&]
        {
          this->_state.reset(
            new surface::gap::State{
              meta_host, meta_port, trophonius_host, trophonius_port,
                apertus_host, apertus_port});
        });
    }

    gap_State():
      _scheduler{},
      _keep_alive{this->_scheduler, "State keep alive",
                  []
                  {
                    while (true)
                    {
                      auto& current = *reactor::Scheduler::scheduler()->current();
                      current.sleep(boost::posix_time::seconds(60));
                    }
                  }},
      _scheduler_thread{
        [&]
        {
          try
          {
            this->_scheduler.run();
          }
          catch (...)
          {
            ELLE_ERR("exception escaped from State scheduler: %s",
                     elle::exception_string());
            this->_exception = std::current_exception();
          }
        }}
    {
      this->_scheduler.mt_run<void>(
        "creating state",
        [&]
        {
          this->_state.reset(new surface::gap::State{});
        });
    }

    ~gap_State()
    {
      elle::Finally sched_destruction{
        [&] ()
        {
          this->_scheduler.mt_run<void>(
            "destroying sched",
            []
            {
              auto& scheduler = *reactor::Scheduler::scheduler();
              scheduler.terminate_now();
            });

          this->_scheduler_thread.join();
        }
      };

      elle::Finally state_destruction{
        [&] ()
        {
          this->_scheduler.mt_run<void>(
            "destroying state",
            [&] () -> void
            {
              this->_state.reset();
            });
        }
      };
    }

  };
}

class _Ret
{
public:
  virtual
  gap_Status
  status() const = 0;
};

template <typename Type>
class Ret: public _Ret
{
public:
  template <typename... Args>
  Ret(Args&&... args):
    _value{std::forward<Args>(args)...}
  {}

  Type
  value() const
  {
    return this->_value.second;
  }

  gap_Status
  status() const
  {
    return this->_value.first;
  }

  operator gap_Status() const
  {
    return this->status();
  }

  operator Type() const
  {
    return this->value();
  }

private:
  std::pair<gap_Status, Type> _value;
};

template <>
class Ret<gap_Status>
{
public:
  template <typename... Args>
  Ret(gap_Status status, Args&&... args):
    _status{status}
  {}

  operator gap_Status() const
  {
    return this->status();
  }

  ELLE_ATTRIBUTE_R(gap_Status, status);
};

template <typename Type>
Ret<Type>
run(gap_State* state,
    std::string const& name,
    std::function<Type (surface::gap::State&)> const& function)
{
  assert(state != nullptr);

  gap_Status ret = gap_ok;
  try
  {
    reactor::Scheduler& scheduler = state->scheduler();

    return Ret<Type>(
      ret,
      scheduler.mt_run<Type>
      (
        name,
        [&] () { return function(state->state()); }
        )
      );
  }
  catch (elle::HTTPException const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    if (err.code == elle::ResponseCode::error)
      ret = gap_network_error;
    else if (err.code == elle::ResponseCode::internal_server_error)
      ret = gap_api_error;
    else
      ret = gap_internal_error;
  }
  catch (plasma::meta::Exception const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    ret = (gap_Status) err.err;
  }
  catch (surface::gap::Exception const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    ret = err.code;
  }
  catch (elle::Exception const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    ret = gap_internal_error;
  }
  catch (std::exception const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    ret = gap_internal_error;
  }
  catch (...)
  {
    ELLE_ERR("%s: unknown error type", name);
    ret = gap_internal_error;
  }
  return Ret<Type>{ret, Type{}};
}

extern "C"
{
  static char**
  _cpp_stringlist_to_c_stringlist(std::list<std::string> const& list)
  {
    size_t total_size = (1 + list.size()) * sizeof(void*);
    for (auto const& str : list)
      total_size += str.size() + 1;

    char** ptr = reinterpret_cast<char**>(malloc(total_size));
    if (ptr == nullptr)
      return nullptr;


    char** array = ptr;
    char* cstr = reinterpret_cast<char*>(ptr + (list.size() + 1));
    for (auto const& str : list)
      {
        *array = cstr;
        ::strncpy(cstr, str.c_str(), str.size() + 1);
        ++array;
        cstr += str.size() + 1;
      }
    *array = nullptr;
    return ptr;
  }

  static char**
  _cpp_stringvector_to_c_stringlist(std::vector<std::string> const& list)
  {
    size_t total_size = (1 + list.size()) * sizeof(void*);
    for (auto const& str : list)
      total_size += str.size() + 1;

    char** ptr = reinterpret_cast<char**>(malloc(total_size));
    if (ptr == nullptr)
      return nullptr;

    char** array = ptr;
    char* cstr = reinterpret_cast<char*>(ptr + (list.size() + 1));
    for (auto const& str : list)
      {
        *array = cstr;
        ::strncpy(cstr, str.c_str(), str.size() + 1);
        ++array;
        cstr += str.size() + 1;
      }
    *array = nullptr;
    return ptr;
  }

  /// - gap ctor & dtor -----------------------------------------------------

  static
  bool
  initialize_lune()
  {
    static bool initialized = false;
    if (!initialized)
    {
      if (lune::Lune::Initialize() == elle::Status::Error)
      {
        ELLE_ERR("Cannot initialize root components");
        return initialized;
      }
      initialized = true;
    }
    return initialized;
  }

  gap_State* gap_new()
  {
    if (!initialize_lune())
      return nullptr;

    try
    {
      gap_State* state = new gap_State();
      return state;
    }
    catch (std::exception const& err)
    {
      ELLE_ERR("Cannot initialize gap state: %s", err.what());
      return nullptr;
    }
    catch (...)
    {
      ELLE_ERR("Cannot initialize gap state");
      return nullptr;
    }
  }

  /// Create a new state.
  /// Returns NULL on failure.
  gap_State* gap_configurable_new(char const* meta_host,
                                  unsigned short meta_port,
                                  char const* trophonius_host,
                                  unsigned short trophonius_port,
                                  char const* apertus_host,
                                  unsigned short apertus_port)
  {
    if (!initialize_lune())
      return nullptr;

    if (!initialize_lune())
      return nullptr;

    try
    {
      gap_State* state = new gap_State(meta_host,
                                       meta_port,
                                       trophonius_host,
                                       trophonius_port,
                                       apertus_host,
                                       apertus_port);
      return state;
    }
    catch (std::exception const& err)
    {
      ELLE_ERR("Cannot initialize gap state: %s", err.what());
      return nullptr;
    }
    catch (...)
    {
      ELLE_ERR("Cannot initialize gap state");
      return nullptr;
    }
  }

  void gap_free(gap_State* state)
  {
    delete state;
  }

  void gap_enable_debug(gap_State* state)
  {
    assert(state != nullptr);
    // FIXME
    //__TO_CPP(state)->log.level(elle::log::Logger::Level::debug);
  }

  gap_Status gap_meta_status(gap_State*)
  {
    return gap_ok;
  }

  char const*
  gap_meta_url(gap_State*)
  {
    static std::string meta_url;
    meta_url = common::meta::url(); // force retreival
    return meta_url.c_str();
  }

  gap_Status
  gap_debug(gap_State* state)
  {
    (void) state;
    return gap_ok;
  }

  //- Authentication ----------------------------------------------------------

  char*
  gap_hash_password(gap_State* state,
                    char const* email,
                    char const* password)
  {
    assert(email != nullptr);
    assert(password != nullptr);

    return run<char*>(state,
                      "hash_password",
                      [&] (surface::gap::State& state) -> char*
                      {
                        std::string h = state.hash_password(email, password);
                        return ::strdup(h.c_str());
                      });
  }

  void gap_hash_free(char* h)
  {
    ::free(h);
  }

  gap_Status
  gap_login(gap_State* _state,
            char const* email,
            char const* hash_password)
  {
    assert(email != nullptr);
    assert(hash_password != nullptr);

    return run<gap_Status>(_state,
                           "login",
                           [&] (surface::gap::State& state) -> gap_Status
                           {
                             state.login(email, hash_password);
                             return gap_ok;
                           });
  }

  gap_Bool
  gap_logged_in(gap_State* state)
  {
    return run<gap_Bool>(state,
                         "logged",
                         [&] (surface::gap::State& state) -> gap_Bool
                         {
                           return state.logged_in();
                         });
  }

  gap_Status
  gap_logout(gap_State* state)
  {
    return run<gap_Status>(state,
                         "logout",
                         [&] (surface::gap::State& state) -> gap_Status
                         {
                           state.logout();
                           return gap_ok;
                         });
  }

  gap_Status
  gap_token(gap_State* state,
            char** usertoken)
  {
    auto ret = run<std::string>(
      state,
      "token",
      [&] (surface::gap::State& state) -> std::string
      {
        return state.token();
      });

    if (ret.status() != gap_ok)
      return ret;

    char* new_token = strdup(ret.value().c_str());
    if (new_token != nullptr)
    {
      *usertoken = new_token;
    }

    return ret;
  }

  gap_Status
  gap_generation_key(gap_State* state,
                     char** usertoken)
  {
    auto ret = run<std::string>(
      state,
      "token",
      [&] (surface::gap::State& state) -> std::string
      {
        return state.token_generation_key();
      });

    if (ret.status() != gap_ok)
      return ret;

    char* new_token = strdup(ret.value().c_str());
    if (new_token != nullptr)
    {
      *usertoken = new_token;
    }

    return ret;
  }

  gap_Status
  gap_register(gap_State* state,
               char const* fullname,
               char const* email,
               char const* password,
               char const* device_name,
               char const* activation_code)
  {
    auto ret = run<gap_Status>(state,
                               "register",
                               [&] (surface::gap::State& state) -> gap_Status
                               {
                                 state.register_(fullname, email, password, activation_code);
                                 return gap_ok;
                               });

    if (ret.status() == gap_ok && device_name != nullptr)
    {
      ret = run<gap_Status>(state,
                            "device",
                            [&] (surface::gap::State& state) -> gap_Status
                            {
                              state.device();
                              return gap_ok;
                            });

    }
    return ret;
  }

  gap_Status
  gap_poll(gap_State* state)
  {
    return run<gap_Status>(state,
                         "poll",
                         [&] (surface::gap::State& state) -> gap_Status
                         {
                           state.notification_manager().poll();
                           return gap_ok;
                         });
  }

  /// - Device --------------------------------------------------------------
  gap_Status
  gap_device_status(gap_State* state)
  {
    return run<gap_Status>(state,
                          "device status",
                          [&] (surface::gap::State& state) -> gap_Status
                          {
                            if (state.has_device())
                              return gap_ok;
                            else
                              return gap_no_device_error;
                          });
  }

  gap_Status
  gap_set_device_name(gap_State* state,
                      char const* name)
  {
    assert(name != nullptr);
    return run<gap_Status>(state,
                           "set device name",
                           [&] (surface::gap::State& state) -> gap_Status
                           {
                             // state.update_device(name);
                             return gap_ok;
                           });
  }

  /// - Self ----------------------------------------------------------------
  char const*
  gap_user_token(gap_State* state)
  {
    return run<char const*>(state,
                            "user token",
                            [&] (surface::gap::State& state) -> char const*
                            {
                              auto token = state.token();
                              return token.c_str();
                            });
  }

  char const*
  gap_self_email(gap_State* state)
  {
    return run<char const*>(state,
                            "user email",
                            [&] (surface::gap::State& state) -> char const*
                            {
                              auto email = state.me().email;
                              return email.c_str();
                            });
  }

  char const*
  gap_self_id(gap_State* state)
  {
    return run<char const*>(state,
                            "user id",
                            [&] (surface::gap::State& state) -> char const*
                            {
                              auto id = state.me().id;
                              return id.c_str();
                            });
  }

  /// Get current user remaining invitations.
  int
  gap_self_remaining_invitations(gap_State* state)
  {
    return run<int>(state,
                    "user invitations",
                    [&] (surface::gap::State& state) -> int
                    {
                      return state.me().remaining_invitations;
                    });
  }

  /// - User ----------------------------------------------------------------

  char const*
  gap_user_directory(gap_State* state, char const** directory)
  {
    return run<char const*>(state,
                            "user directory",
                            [&] (surface::gap::State& state) -> char const*
                            {
                              std::string path = state.user_directory();
                              char const* tmp = strdup(path.c_str());
                              if (directory != nullptr)
                              {
                                *directory = tmp;
                              }
                              return tmp;
                            });
  }

  char const* gap_user_fullname(gap_State* state, char const* id)
  {
    return run<char const*>(state,
                            "user fullname",
                            [&] (surface::gap::State& state) -> char const*
                            {
                              auto const& user = state.user_manager().one(id);
                              return user.fullname.c_str();
                            });
  }

  char const* gap_user_handle(gap_State* state, char const* id)
  {
    assert(id != nullptr);
    return run<char const*>(state,
                            "user handle",
                            [&] (surface::gap::State& state) -> char const*
                            {
                              auto const& user = state.user_manager().one(id);
                              return user.handle.c_str();
                            });
  }

  gap_Status
  gap_user_icon(gap_State* state,
                char const* user_id,
                void** data,
                size_t* size)
  {
    assert(user_id != nullptr);
    *data = nullptr;
    *size = 0;
    return run<gap_Status>(state,
                           "user icon",
                            [&] (surface::gap::State& state) -> gap_Status
                            {
                              auto pair = state.user_manager().icon(user_id).release();
                              *data = pair.first.release();
                              *size = pair.second;
                              return gap_ok;
                            });
  }

  void
  gap_user_icon_free(void* data)
  {
    free(data);
  }

  char const*
  gap_user_by_email(gap_State* state, char const* email)
  {
    assert(email != nullptr);
    return run<char const*>(state,
                            "user by email",
                            [&] (surface::gap::State& state) -> char const*
                            {
                              auto const& user = state.user_manager().one(email);
                              return user.id.c_str();
                            });
  }

  char**
  gap_search_users(gap_State* state, char const* text)
  {
    assert(text != nullptr);
    auto ret = run<std::map<std::string, surface::gap::User>>(
      state,
      "users",
      [&] (surface::gap::State& state) -> std::map<std::string, surface::gap::User>
      {
        return state.user_manager().search(text);
      });

    if (ret.status() != gap_ok)
      return nullptr;

    std::vector<std::string> result;
    for (auto const& pair : ret.value())
      result.push_back(pair.first);
    return _cpp_stringvector_to_c_stringlist(result);
  }

  void gap_search_users_free(char** users)
  {
    ::free(users);
  }

  gap_UserStatus
  gap_user_status(gap_State* state, char const* user_id)
  {
    assert(user_id != nullptr);

    return run<gap_UserStatus>(
      state,
      "user status",
      [&] (surface::gap::State& state) -> gap_UserStatus
      {
        return (gap_UserStatus) state.user_manager().one(user_id).status;
      });
  }

  char**
  gap_swaggers(gap_State* state)
  {
    auto ret = run<surface::gap::UserManager::SwaggerSet>(
      state,
      "swaggers",
      [&] (surface::gap::State& state) -> surface::gap::UserManager::SwaggerSet
      {
        return state.user_manager().swaggers();
      });

    if (ret.status() != gap_ok)
      return nullptr;

    std::vector<std::string> result;
    for (auto const& id : ret.value())
      result.push_back(id);
    return _cpp_stringvector_to_c_stringlist(result);
  }

  void
  gap_swaggers_free(char** swaggers)
  {
    ::free(swaggers);
  }

  /// - Permissions ---------------------------------------------------------
  static inline
  void
  gap_file_users_free(char** users)
  {
    ::free(users);
  }

  // - Trophonius -----------------------------------------------------------

  gap_Status
  gap_new_swagger_callback(gap_State* state,
                           gap_new_swagger_callback_t cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (NewSwaggerNotification const& notif) {
      cb(notif.user_id.c_str());
    };

    return run<gap_Status>(
      state,
      "new swagger callback",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.notification_manager().new_swagger_callback(cpp_cb);
        return gap_ok;
      });
  }

  gap_Status
  gap_user_status_callback(gap_State* state,
                           gap_user_status_callback_t cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (UserStatusNotification const& notif) {
        cb(notif.user_id.c_str(), (gap_UserStatus) notif.status);
    };

    return run<gap_Status>(
      state,
      "user status callback",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.notification_manager().user_status_callback(cpp_cb);
        return gap_ok;
      });

  }

  gap_Status
  gap_transaction_callback(gap_State* state,
                           gap_transaction_callback_t cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (TransactionNotification const& notif, bool is_new) {
        cb(notif.id.c_str(), is_new);
    };

    return run<gap_Status>(
      state,
      "transaction callback",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.notification_manager().transaction_callback(cpp_cb);
        return gap_ok;
      });
  }

  gap_Status
  gap_message_callback(gap_State* state,
                       gap_message_callback_t cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (MessageNotification const& notif) {
        cb(notif.sender_id.c_str(), notif.message.c_str());
    };

    return run<gap_Status>(
      state,
      "message callback",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.notification_manager().message_callback(cpp_cb);
        return gap_ok;
      });
  }

  gap_Status
  gap_on_error_callback(gap_State* state,
                        gap_on_error_callback_t cb)
  {
    auto cpp_cb = [cb] (gap_Status s,
                        std::string const& str,
                        std::string const& tid)
    {
      cb(s, str.c_str(), tid.c_str());
    };

    return run<gap_Status>(
      state,
      "error callback",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.notification_manager().on_error_callback(cpp_cb);
        return gap_ok;
      });
  }

  /// Transaction getters.
#define DEFINE_TRANSACTION_GETTER(_type_, _field_, _transform_)               \
  _type_                                                                      \
  gap_transaction_ ## _field_(gap_State* state,                               \
                              char const* _id)                                \
  {                                                                           \
    assert(_id != nullptr);                                                   \
    return run<_type_>(                                          \
      state,                                                                  \
      #_field_,                                                               \
      [&] (surface::gap::State& state) -> _type_                              \
      {                                                                       \
        return _transform_(state.transaction_manager().one(_id)._field_);     \
      });                                                                    \
  }                                                                           \
  /**/

#define NO_TRANSFORM
#define GET_CSTR(_expr_) (_expr_).c_str()

#define DEFINE_TRANSACTION_GETTER_STR(_field_)                                \
  DEFINE_TRANSACTION_GETTER(char const*, _field_, GET_CSTR)                   \
/**/
#define DEFINE_TRANSACTION_GETTER_INT(_field_)                                \
  DEFINE_TRANSACTION_GETTER(int, _field_, NO_TRANSFORM)                       \
/**/
#define DEFINE_TRANSACTION_GETTER_DOUBLE(_field_)                             \
  DEFINE_TRANSACTION_GETTER(double, _field_, NO_TRANSFORM)                    \
/**/
#define DEFINE_TRANSACTION_GETTER_BOOL(_field_)                               \
  DEFINE_TRANSACTION_GETTER(gap_Bool, _field_, NO_TRANSFORM)                  \
/**/

  DEFINE_TRANSACTION_GETTER_STR(sender_id)
  DEFINE_TRANSACTION_GETTER_STR(sender_fullname)
  DEFINE_TRANSACTION_GETTER_STR(sender_device_id)
  DEFINE_TRANSACTION_GETTER_STR(recipient_id)
  DEFINE_TRANSACTION_GETTER_STR(recipient_fullname)
  DEFINE_TRANSACTION_GETTER_STR(recipient_device_id)
  DEFINE_TRANSACTION_GETTER_STR(network_id)
  DEFINE_TRANSACTION_GETTER_STR(first_filename)
  DEFINE_TRANSACTION_GETTER_STR(message)
  DEFINE_TRANSACTION_GETTER_INT(files_count)
  DEFINE_TRANSACTION_GETTER_INT(total_size)
  DEFINE_TRANSACTION_GETTER_DOUBLE(timestamp)
  DEFINE_TRANSACTION_GETTER_BOOL(is_directory)
  // _transform_ is a cast from plasma::TransactionStatus
  DEFINE_TRANSACTION_GETTER(gap_TransactionStatus,
                            status,
                            (gap_TransactionStatus))

  float
  gap_transaction_progress(gap_State* state,
                           char const* transaction_id)
  {
    assert(transaction_id != nullptr);

    return run<float>(
      state,
      "progress",
      [&] (surface::gap::State& state) -> float
      {
        // state.transaction_manager().progress(transaction_id);
        return gap_ok;
      });
  }

  gap_Status
  gap_transaction_sync(gap_State* state,
                         char const* transaction_id)
  {
    assert(transaction_id != nullptr);

    return run<gap_Status>(
      state,
      "sync",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.transaction_manager().sync(transaction_id);
        return gap_ok;
      });
  }

  // - Notifications -----------------------------------------------------------

  gap_Status
  gap_pull_notifications(gap_State* state,
                         int count,
                         int offset)
  {
    return run<gap_Status>(
      state,
      "pull",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.notification_manager().pull(count, offset, false);
        return gap_ok;
      });
  }

  gap_Status
  gap_pull_new_notifications(gap_State* state,
                             int count,
                             int offset)
  {
    return run<gap_Status>(
      state,
      "pull new",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.notification_manager().pull(count, offset, true);
        return gap_ok;
      });
  }

  gap_Status
  gap_notifications_read(gap_State* state)
  {
    return run<gap_Status>(
      state,
      "read",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.notification_manager().read();
        return gap_ok;
      });
  }

  char**
  gap_transactions(gap_State* state)
  {
    assert(state != nullptr);

    auto ret = run<surface::gap::TransactionManager::TransactionsMap>(
      state,
      "read",
      [&] (surface::gap::State& state) -> surface::gap::TransactionManager::TransactionsMap
      {
        return state.transaction_manager().all();
      });

    if (ret.status() != gap_ok)
      return nullptr;

    std::vector<std::string> res;

    for (auto const& transaction_pair : ret.value())
      res.push_back(transaction_pair.first);

    ELLE_DEBUG("gap_transactions() = %s", res);
    return _cpp_stringvector_to_c_stringlist(res);
  }

  void gap_transactions_free(char** transactions)
  {
    ::free(transactions);
  }

  uint32_t
  gap_send_files(gap_State* state,
                 char const* recipient_id,
                 char const* const* files)
  {
    assert(recipient_id != nullptr);
    assert(files != nullptr);

    std::unordered_set<std::string> s;

    while (*files != nullptr)
    {
      s.insert(*files);
      ++files;
    }

    return run<uint32_t>(
      state,
      "send files",
      [&] (surface::gap::State& state) -> uint32_t
      {
        return state.send_files(recipient_id, std::move(s));
      });
  }

  uint32_t
  gap_cancel_transaction(gap_State* state,
                         char const* transaction_id)
  {
    assert(transaction_id != nullptr);
    return run<uint32_t>(
      state,
      "cancel transaction",
      [&] (surface::gap::State& state) -> uint32_t
      {
        return state.cancel_transaction(transaction_id);
      });
  }

  uint32_t
  gap_reject_transaction(gap_State* state,
                         char const* transaction_id)
  {
    assert(transaction_id != nullptr);
    return run<uint32_t>(
      state,
      "reject transaction",
      [&] (surface::gap::State& state) -> uint32_t
      {
        return state.reject_transaction(transaction_id);
      });
  }

  uint32_t
  gap_accept_transaction(gap_State* state,
                         char const* transaction_id)
  {
    assert(transaction_id != nullptr);
    return run<uint32_t>(
      state,
      "accept transaction",
      [&] (surface::gap::State& state) -> uint32_t
      {
        return state.accept_transaction(transaction_id);
      });
  }

  gap_Status
  gap_join_transaction(gap_State* state,
                       char const* transaction_id)
  {
    assert(transaction_id != nullptr);
    return run<gap_Status>(
      state,
      "join transaction",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.join_transaction(transaction_id);
        return gap_ok;
      });
  }

  gap_Status
  gap_set_output_dir(gap_State* state,
                     char const* output_path)
  {
    assert(output_path != nullptr);

    return run<gap_Status>(
      state,
      "set output dir",
      [&] (surface::gap::State& state) -> gap_Status
      {
        state.output_dir(output_path);
        return gap_ok;
      });
  }

  char const*
  gap_get_output_dir(gap_State* state)
  {
    assert(state != nullptr);

    auto ret = run<std::string>(
      state,
      "join transaction",
      [&] (surface::gap::State& state) -> std::string
      {
        return state.output_dir();
      });

    return ret.value().c_str();
  }

  static
  std::string
  read_file(std::string const& filename)
  {
    std::stringstream file_content;

    file_content <<  ">>> " << filename << std::endl;

    std::ifstream f(filename);
    std::string line;
    while (f.good() && !std::getline(f, line).eof())
      file_content << line << std::endl;
    file_content << "<<< " << filename << std::endl;
    return file_content.str();
  }

  void
  gap_send_file_crash_report(char const* module,
                             char const* filename)
  {
    try
    {
      std::string file_content;
      if (filename != nullptr)
        file_content = read_file(filename);
      else
        file_content = "<<< No file was specified!";

      elle::crash::report(common::meta::host(),
                          common::meta::port(),
                          (module != nullptr ? module : "(nil)"),
                          "Crash",
                          elle::Backtrace::current(),
                          file_content);
    }
    catch (...)
    {
      ELLE_WARN("cannot send crash reports: %s", elle::exception_string());
    }
  }

  gap_Status
  gap_send_last_crash_logs(char const* _crash_report, char const* _state_log)
  {
    try
    {
      std::string const infinit_dir = common::infinit::home();
      std::string const crash_report{_crash_report,
                                     _crash_report + strlen(_crash_report)};
      std::string const state_log{_state_log,
                                  _state_log + strlen(_state_log)};
      std::string const finder_log{"finder-plugin.log"};
      std::string const crash_report_path = infinit_dir + "/" + crash_report;
      std::string const state_log_path = infinit_dir + "/" + state_log;
      std::string const finder_log_path = "/tmp/" + finder_log;

      std::string const crash_archive = "/tmp/infinit-crash-archive";

      std::list<std::string> args{"cjf", crash_archive};
      if (boost::filesystem::exists(state_log_path))
      {
        args.push_back("-C");
        args.push_back(infinit_dir);
        args.push_back(state_log);
      }
      if (boost::filesystem::exists(crash_report_path))
      {
        args.push_back("-C");
        args.push_back(infinit_dir);
        args.push_back(crash_report);
      }
      if (boost::filesystem::exists(finder_log_path))
      {
        args.push_back("-C");
        args.push_back("/tmp");
        args.push_back(finder_log);
      }

      if (args.size() > 3)
      {
        elle::system::Process tar{"tar", args};
        tar.wait();
#if defined(INFINIT_LINUX)
        std::string b64 = elle::system::check_output("base64",
                                                     "-w0",
                                                     crash_archive);
#else
        std::string b64 = elle::system::check_output("base64", crash_archive);
#endif
        auto title = elle::sprintf("Application Crash Report");
        elle::crash::report(common::meta::host(),
                            common::meta::port(),
                            "Logs", title,
                            elle::Backtrace::current(),
                            "Logs attached",
                            b64);
      }
      else
      {
        ELLE_WARN("no logs to send");
      }
    }
    catch (...)
    {
      ELLE_WARN("cannot send crash reports: %s", elle::exception_string());
      return gap_api_error;
    }
    return gap_ok;
  }

  gap_Status
  gap_gather_crash_reports(char const* _user_id,
                           char const* _network_id)
  {
    try
    {
      namespace fs = boost::filesystem;
      std::string const user_id{_user_id,
                                _user_id + strlen(_user_id)};
      std::string const network_id{_network_id,
                                   _network_id + strlen(_network_id)};

      std::string const user_dir = common::infinit::user_directory(user_id);
      std::string const network_dir =
        common::infinit::network_directory(user_id, network_id);

      fs::directory_iterator ndir;
      fs::directory_iterator udir;
      try
      {
        udir = fs::directory_iterator{user_dir};
        ndir = fs::directory_iterator{network_dir};
      }
      catch (fs::filesystem_error const& e)
      {
        return gap_Status::gap_file_not_found;
      }

      boost::iterator_range<fs::directory_iterator> user_range{
        udir, fs::directory_iterator{}};
      boost::iterator_range<fs::directory_iterator> network_range{
        ndir, fs::directory_iterator{}};

      std::vector<fs::path> logs;
      for (auto const& dir_ent: boost::join(user_range, network_range))
      {
        auto const& path = dir_ent.path();

        if (path.extension() == ".log")
          logs.push_back(path);

      }
      std::string filename = elle::sprintf("/tmp/infinit-%s-%s",
                                           user_id, network_id);
      std::list<std::string> args{"cjf", filename};
      for (auto const& log: logs)
        args.push_back(log.string());

      elle::system::Process tar{"tar", args};
      tar.wait();
#if defined(INFINIT_LINUX)
      std::string b64 = elle::system::check_output("base64", "-w0", filename);
#else
      std::string b64 = elle::system::check_output("base64", filename);
#endif

      auto title = elle::sprintf("Crash: Logs file for user: %s, network: %s",
                                 user_id, network_id);
      elle::crash::report(common::meta::host(),
                          common::meta::port(),
                          "Logs", title,
                          elle::Backtrace::current(),
                          "Logs attached",
                          b64);
    }
    catch (...)
    {
      ELLE_WARN("cannot send crash reports: %s", elle::exception_string());
      return gap_error;
    }
    return gap_ok;
  }

  // Generated file.
  #include <surface/gap/gen_metrics.hh>

} // ! extern "C"
