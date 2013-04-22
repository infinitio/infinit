#include "State.hh"

#include "_detail/Operation.hh"
#include "_detail/TransactionProgress.hh"

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
    State::State()
      : _logger_intializer{}
      , _meta{common::meta::host(), common::meta::port(), true}
      , _files_infos{}
    {
      ELLE_LOG("Creating a new State");

      // Start metrics after setting up the logger.
      reporter().start();
      google_reporter().start();

      std::string user = elle::os::getenv("INFINIT_USER", "");

      if (user.length() > 0)
      {
        std::string identity_path = common::infinit::identity_path(user);

        if (identity_path.length() > 0 && fs::exists(identity_path))
        {
          std::ifstream identity;
          identity.open(identity_path);

          if (!identity.good())
            return;

          std::string token;
          std::getline(identity, token);

          std::string ident;
          std::getline(identity, ident);

          std::string mail;
          std::getline(identity, mail);

          std::string id;
          std::getline(identity, id);

          this->_meta.token(token);
          this->_meta.identity(ident);
          this->_meta.email(mail);

          this->_me = this->_meta.self();
        }
      }

      // Initialize google metrics.
      auto const& g_info = common::metrics::google_info();
      elle::metrics::google::register_service(google_reporter(),
                                              g_info.server,
                                              g_info.port,
                                              g_info.id_path);

      // Initialize server.
      auto const& km_info = common::metrics::km_info();
      elle::metrics::kissmetrics::register_service(reporter(),
                                                   km_info.server,
                                                   km_info.port,
                                                   km_info.id_path);
    }

    State::State(std::string const& token):
      State{}
    {
      ELLE_LOG("Creating a new State with token");
      this->_meta.token(token);
      auto res = this->_meta.self();
      this->_meta.identity(res.identity);
      this->_meta.email(res.email);
      //XXX factorize that shit
      this->_me = res;
    }

    State::~State()
    {
      ELLE_WARN("Destroying state.");
      this->logout();
    }

    std::string const&
    State::token()
    {
      return this->_meta.token();
    }

    Self const&
    State::me()
    {
      return this->_me;
    }

    void
    State::login(std::string const& email,
                 std::string const& password)
    {
      this->_meta.token("");

      std::string lower_email = email;

      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      reporter().store("user_login",
                       {{MKey::status, "attempt"}});

      plasma::meta::LoginResponse res;
      try
      {
        res = this->_meta.login(lower_email, password);
      }
      CATCH_FAILURE_TO_METRICS("user_login");

      reporter().update_user(res.id);
      reporter().store("user_login",
                            {{MKey::status, "succeed"}});

      // XXX: Not necessary but better.
      google_reporter().update_user(res.id);
      google_reporter().store("user:login:succeed",
                                  {{MKey::session, "start"},
                                   {MKey::status, "succeed"}});


      ELLE_DEBUG("Logged in as %s token = %s", email, res.token);

      this->_me = this->_meta.self();

      ELLE_DEBUG("id: '%s' - fullname: '%s' - lower_email: '%s'",
                 this->_me.id,
                 this->_me.fullname,
                 this->_me.email);

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

        dictionary.store(res.id);
      }

      std::ofstream identity_infos{common::infinit::identity_path(res.id)};

      if (!identity_infos.good())
      {
        ELLE_ERR("Cannot open identity file");
      }

      identity_infos << res.token << "\n"
                     << res.identity << "\n"
                     << res.email << "\n"
                     << res.id << "\n"
                     ;

      if (!identity_infos.good())
      {
        ELLE_ERR("Cannot write identity file");
      }
      identity_infos.close();

      this->_notification_manager.reset(new NotificationManager(this->_meta,
                                                                this->_me));
      this->_user_manager.reset(new UserManager(*this->_notification_manager,
                                                this->_meta,
                                                this->_me));

      this->device_id();

      this->_network_manager.reset(new NetworkManager(//*this->_notification_manager,
                                                      this->_meta,
                                                      this->_me,
                                                      this->_device));

      this->_transaction_manager.reset(
        new TransactionManager(*this->_network_manager,
                               *this->_notification_manager,
                               this->_meta,
                               this->_me,
                               this->_device));
    }

    void
    State::logout()
    {
      if (this->_meta.token().empty())
        return;

      this->_transaction_manager.reset();
      this->_network_manager.reset();
      this->_user_manager.reset();
      this->_notification_manager.reset();

      // End session the session.
      reporter().store("user_logout",
                       {{MKey::status, "attempt"}});

      // XXX: Not necessary but better.
      google_reporter().store("user:logout:attempt",
                              {{MKey::session, "end"},});


      try
      {
        this->_meta.logout();
      }
      CATCH_FAILURE_TO_METRICS("user_logout");

      // End session the session.
      reporter().store("user_logout",
                       {{MKey::status, "succeed"}});

      // XXX: Not necessary but better.
      google_reporter().store("user:logout:succeed");
    }

    std::string
    State::hash_password(std::string const& email,
                         std::string const& password)
    {
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
      // End session the session.
      reporter().store("user_register",
                            {{MKey::status, "attempt"}});


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
      reporter().store("user_register",
                       {{MKey::status, "succeed"}});


      ELLE_DEBUG("Registered new user %s <%s>", fullname, lower_email);
      this->login(lower_email, password);
    }

  }
}
