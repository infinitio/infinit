#include "State.hh"

#include <common/common.hh>

// #include <etoile/portal/Portal.hh>
#include <lune/Identity.hh>
#include <lune/Dictionary.hh>

#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <elle/serialize/HexadecimalArchive.hh>

#include <surface/gap/metrics.hh>
#include <metrics/_details/google.hh>
#include <metrics/_details/kissmetrics.hh>


// #include <elle/memory.hh>
#include <boost/filesystem.hpp>


#include <fstream>
//#include <iterator>

#include <openssl/sha.h>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;

    LoggerInitializer::LoggerInitializer()
    {
      ELLE_TRACE_METHOD("");

      std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");
      if (!log_file.empty())
      {
        if (elle::os::in_env("INFINIT_LOG_FILE_PID"))
        {
          log_file += ".";
          log_file += std::to_string(::getpid());
        }

        static std::ofstream out{
          log_file + ".log",
          std::fstream::trunc | std::fstream::out};

        elle::log::logger(
          std::unique_ptr<elle::log::Logger>{new elle::log::TextLogger(out)});
      }
    }

    // - State ----------------------------------------------------------------
    State::State(std::string const& meta_host,
                 uint16_t meta_port,
                 std::string const& trophonius_host,
                 uint16_t trophonius_port):
      _logger_intializer{},
      _meta{meta_host, meta_port, true},
      _reporter(),
      _google_reporter(),
      _me{nullptr},
      _device{nullptr},
      _files_infos{},
      _trophonius_host{trophonius_host},
      _trophonius_port{trophonius_port}
    {
      ELLE_TRACE_SCOPE("%s: create state", *this);

      // Start metrics after setting up the logger.
      this->_reporter.start();
      this->_google_reporter.start();

      std::string token_path = elle::os::getenv("INFINIT_TOKEN_FILE", "");

      if (!token_path.empty() && elle::os::path::exists(token_path))
      {
        std::string const token_genkey = [&] () -> std::string
        {
          ELLE_DEBUG("read generation token from %s", token_path);
          std::ifstream token_file{token_path};

          std::string _token_genkey;
          std::getline(token_file, _token_genkey);
          return _token_genkey;
        }();

        ELLE_TRACE_SCOPE("loading token generating key: %s", token_genkey);
        this->_meta.generate_token(token_genkey);
        this->_me.reset(new Self{this->_meta.self()});

        std::ofstream identity_infos{
          common::infinit::identity_path(this->me().id)};

        if (!identity_infos.good())
        {
          ELLE_ERR("Cannot open identity file");
        }

        identity_infos << this->_meta.token() << "\n"
                       << this->me().identity << "\n"
                       << this->me().email << "\n"
                       << this->me().id << "\n"
        ;
        if (!identity_infos.good())
        {
          ELLE_ERR("Cannot write identity file");
        }
        identity_infos.close();
      }

      // Initialize google metrics.
      elle::metrics::google::register_service(this->_google_reporter);
      // Initialize server.
      elle::metrics::kissmetrics::register_service(this->_reporter);
    }

    std::string const&
    State::token_generation_key() const
    {
      ELLE_TRACE_SCOPE("%s: generate token", *this);

      return this->me().token_generation_key;
    }

    std::string
    State::user_directory()
    {
      ELLE_TRACE_METHOD("");

      return common::infinit::user_directory(this->me().id);
    }

    State::~State()
    {
      ELLE_TRACE_SCOPE("%s: destroying state", *this);

      ELLE_SCOPE_EXIT([&] {
        try
        {
          this->logout();
        }
        catch (...)
        {
          ELLE_WARN("Couldn't logout: %s", elle::exception_string());
        }
      });
    }

    std::string const&
    State::token()
    {
      return this->_meta.token();
    }

    Self const&
    State::me() const
    {
      if (!this->logged_in())
        throw Exception{gap_internal_error, "you must be logged in"};

      if (this->_me == nullptr)
      {
        ELLE_TRACE("loading self info")
          this->_me.reset(new Self{this->_meta.self()});
      }
      ELLE_ASSERT_NEQ(this->_me, nullptr);
      return *this->_me;
    }

    void
    State::login(std::string const& email,
                 std::string const& password)
    {
      ELLE_TRACE_METHOD("%s: login to meta as %s", *this, email);

      this->_meta.token("");
      this->_cleanup();

      std::string lower_email = email;

      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      this->_reporter.store("user_login_attempt");

      plasma::meta::LoginResponse res;
      try
      {
        res = this->_meta.login(lower_email, password);
        ELLE_LOG("Logged in as %s token = %s", email, res.token);
      }
      CATCH_FAILURE_TO_METRICS("user_login");

      this->_reporter.update_user(res.id);
      this->_reporter.store("user_login_succeed");

      // XXX: Not necessary but better.
      this->_google_reporter.update_user(res.id);
      this->_google_reporter.store("user:login:succeed",
                                  {{MKey::session, "start"},
                                   {MKey::status, "succeed"}});

      ELLE_WARN("id: '%s' - fullname: '%s' - lower_email: '%s'",
                 this->me().id,
                 this->me().fullname,
                 this->me().email);

      std::string identity_clear;

      lune::Identity identity;

      // Decrypt the identity
      if (identity.Restore(res.identity)    == elle::Status::Error ||
          identity.Decrypt(password)        == elle::Status::Error ||
          identity.Clear()                  == elle::Status::Error ||
          identity.Save(identity_clear)     == elle::Status::Error)
        throw Exception(gap_internal_error,
                        "Couldn't decrypt the identity file !");

      // Store the identity
      {
        if (identity.Restore(identity_clear)  == elle::Status::Error)
          throw Exception(gap_internal_error,
                          "Cannot save the identity file.");

        identity.store();

        // user.dic
        lune::Dictionary dictionary;

        dictionary.store(this->me().id);
      }

      std::ofstream identity_infos{common::infinit::identity_path(res.id)};

      if (identity_infos.good())
      {
        identity_infos << res.token << "\n"
                       << res.identity << "\n"
                       << res.email << "\n"
                       << res.id << "\n"
                       ;
        identity_infos.close();
      }

      // this->device();
      // this->notification_manager();
      // this->user_manager();
      // this->network_manager();
      this->transaction_manager();
      // this->transaction_manager();
    }

    void
    State::_cleanup()
    {
      ELLE_TRACE_SCOPE("%s: cleaning up the state", *this);

      this->_transaction_manager->reset();
      this->_network_manager->reset();
      this->_user_manager->reset();
      this->_notification_manager->reset();

      this->_device.reset();
      this->_me.reset();
    }

    void
    State::logout()
    {
      ELLE_TRACE_METHOD("");

      if (this->_meta.token().empty())
        return;

      // End session the session.
      this->_reporter.store("user_logout_attempt");

      // XXX: Not necessary but better.
      this->_google_reporter.store("user:logout:attempt",
                                   {{MKey::session, "end"},});

      try
      {
        try
        {
          this->_meta.logout();
        }
        catch (...)
        {
          ELLE_WARN("logout failed, ignore");
          this->_meta.token("");
        }
        this->_cleanup();
      }
      CATCH_FAILURE_TO_METRICS("user_logout");

      // End session the session.
      this->_reporter.store("user_logout_succeed");

      // XXX: Not necessary but better.
      this->_google_reporter.store("user:logout:succeed");
    }

    std::string
    State::hash_password(std::string const& email,
                         std::string const& password)
    {
      // !WARNING! Do not log the password.
      ELLE_TRACE_METHOD(email);

      std::string lower_email = email;

      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      unsigned char hash[SHA256_DIGEST_LENGTH];
      SHA256_CTX context;
      std::string to_hash = lower_email + "MEGABIET" + password + lower_email + "MEGABIET";

      if (SHA256_Init(&context) == 0 ||
          SHA256_Update(&context, to_hash.c_str(), to_hash.size()) == 0 ||
          SHA256_Final(hash, &context) == 0)
        throw Exception(gap_internal_error, "Cannot hash login/password");

      std::ostringstream out;
      elle::serialize::OutputHexadecimalArchive ar(out);

      ar.SaveBinary(hash, SHA256_DIGEST_LENGTH);

      return out.str();
    }

    void
    State::register_(std::string const& fullname,
                     std::string const& email,
                     std::string const& password,
                     std::string const& activation_code)
    {
      // !WARNING! Do not log the password.
      ELLE_TRACE_SCOPE("%s: register as %s: email %s and activation_code %s",
                       *this, fullname, email, activation_code);

      // End session the session.
      this->_reporter.store("user_register_attempt");

      std::string lower_email = email;

      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      // Logout first, and ignore errors.
      try { this->logout(); } catch (elle::HTTPException const&) {}

      try
      {
        this->_meta.register_(lower_email, fullname, password, activation_code);
      }
      CATCH_FAILURE_TO_METRICS("user_register");

      // Send file request successful.
      this->_reporter.store("user_register_succeed");

      ELLE_DEBUG("Registered new user %s <%s>", fullname, lower_email);
      this->login(lower_email, password);
    }

    NetworkManager&
    State::network_manager()
    {
      return this->_network_manager(
        [this] (NetworkManagerPtr& manager) -> NetworkManager& {
          if (manager == nullptr)
          {
            ELLE_TRACE_SCOPE("%s: allocating a new network manager", *this);

            manager.reset(
              new NetworkManager{
                this->_meta,
                this->_reporter,
                this->_google_reporter,
                [this] () -> Self const& { return this->me(); },
                [this] () -> Device const& { return this->device(); },
              });
          }
          return *manager;
        });
    }

    NotificationManager&
    State::notification_manager()
    {
      return this->_notification_manager(
        [this] (NotificationManagerPtr& manager) -> NotificationManager& {
          if (manager == nullptr)
          {
            ELLE_TRACE_SCOPE("%s: allocating a new notification manager", *this);

            manager.reset(
              new NotificationManager{
                this->_trophonius_host,
                this->_trophonius_port,
                this->_meta,
                [this] () -> Self const& { return this->me(); },
                [this] () -> Device const& { return this->device(); },
              });
          }
          return *manager;
        });
    }

    UserManager&
    State::user_manager()
    {
      return this->_user_manager(
        [this] (UserManagerPtr& manager) -> UserManager& {
          if (manager == nullptr)
          {
            ELLE_TRACE_SCOPE("%s: allocating a new user manager", *this);

            manager.reset(
              new UserManager{
                this->notification_manager(),
                this->_meta,
                [this] () -> Self const& { return this->me(); }
              });
          }
          return *manager;
        });
    }

    TransactionManager&
    State::transaction_manager()
    {
      return this->_transaction_manager(
        [this] (TransactionManagerPtr& manager) -> TransactionManager& {
          if (manager == nullptr)
          {
            ELLE_TRACE_SCOPE("%s: allocating a new transaction manager", *this);

            manager.reset(
              new TransactionManager{
                this->notification_manager(),
                this->network_manager(),
                this->user_manager(),
                this->_meta,
                this->_reporter,
                [this] () -> Self const& { return this->me(); },
                [this] () -> Device const& { return this->device(); },
                [this] (unsigned int remaining_invitations) {
                  if (this->_me != nullptr)
                    this->_me->remaining_invitations = remaining_invitations;
                },
              });
          }
          return *manager;
        });
    }

    /*----------.
    | Printable |
    `----------*/
    void
    State::print(std::ostream& stream) const
    {
      stream << "state(" << this->_meta.host() << ":" << this->_meta.port();
      if (!this->_meta.email().empty())
        stream << " as " << this->_meta.email();
      stream << ")";
    }

  }
}
