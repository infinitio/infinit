#include "State.hh"
#include <metrics/Kind.hh>

#include <surface/gap/TransferMachine.hh>
#include <surface/gap/SendMachine.hh>
#include <surface/gap/ReceiveMachine.hh>

#include <common/common.hh>

#include <papier/Identity.hh>
#include <lune/Dictionary.hh>

#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <elle/serialize/HexadecimalArchive.hh>

#include <surface/gap/metrics.hh>
#include <metrics/services/Google.hh>
#include <metrics/services/KISSmetrics.hh>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <fstream>

#include <openssl/sha.h>

#include <version.hh>

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
      ELLE_LOG("Infinit Version: %s", INFINIT_VERSION);
    }

    namespace
    {
      template <metrics::Kind kind, typename Service>
      struct MetricKindService:
          public Service
      {
        template <typename... Args>
        MetricKindService(Args&&... args):
          Service{std::forward<Args>(args)...}
        {}
        static
        std::string
        _prefix()
        {
          switch (kind)
          {
          case metrics::Kind::all:
            return "";
          case metrics::Kind::user:
            return "user";
          case metrics::Kind::network:
            return "network";
          case metrics::Kind::transaction:
            return "transaction";
          }
          elle::unreachable();
        }

        void
        _send(metrics::TimeMetricPair metric) override
        {
          static const std::string prefix = _prefix();
          if (prefix.empty() or
              boost::starts_with(metric.second.at(MKey::tag), prefix))
            Service::_send(std::move(metric));
        }
      };
    }

    State::ConnectionStatus::ConnectionStatus(bool status):
      status(status)
    {}

    Notification::Type State::ConnectionStatus::type = NotificationType_ConnectionStatus;

    /*-------------------------.
    | Construction/Destruction |
    `-------------------------*/
    State::State(std::string const& meta_host,
                 uint16_t meta_port,
                 std::string const& trophonius_host,
                 uint16_t trophonius_port,
                 std::string const& apertus_host,
                 uint16_t apertus_port):
      _logger_intializer{},
      _meta{meta_host, meta_port, true},
      _trophonius{trophonius_host, trophonius_port, [this] (bool status)
        {
          this->on_connection_changed(status);
      }},
      _reporter{common::metrics::fallback_path()},
      _google_reporter{common::metrics::google_fallback_path()},
      _me{nullptr},
      _output_dir{common::system::download_directory()},
      _device{nullptr}
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
        this->_me.reset(new Self{this->meta().self()});

        std::ofstream identity_infos{
          common::infinit::identity_path(this->me().id)};

        if (!identity_infos.good())
        {
          ELLE_ERR("Cannot open identity file");
        }

        identity_infos << this->meta().token() << "\n"
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

      {
        using metrics::services::Google;
        using metrics::services::KISSmetrics;

        this->_google_reporter.add_service_class<Google>(common::metrics::google_info_investors());

        this->_reporter.add_service_class<Google>(common::metrics::google_info());
        this->_reporter.add_service_class<KISSmetrics>(common::metrics::kissmetrics_info());

        typedef MetricKindService<metrics::Kind::user, KISSmetrics> KMUser;
        this->_reporter.add_service_class<KMUser>(
          common::metrics::kissmetrics_info(metrics::Kind::user));
        typedef MetricKindService<metrics::Kind::network, KISSmetrics> KMNetwork;
        this->_reporter.add_service_class<KMNetwork>(
          common::metrics::kissmetrics_info(metrics::Kind::network));

        typedef MetricKindService<metrics::Kind::transaction, KISSmetrics> KMTransaction;
        this->_reporter.add_service_class<KMTransaction>(
          common::metrics::kissmetrics_info(metrics::Kind::transaction));
      }
    }

    State::~State()
    {
      ELLE_TRACE_SCOPE("%s: destroying state", *this);

      try
      {
        this->logout();
      }
      catch (std::runtime_error const&)
      {
        ELLE_WARN("Couldn't logout: %s", elle::exception_string());
      }
    }

    plasma::meta::Client const&
    State::meta(bool authentication_required) const
    {
      if (authentication_required && this->_meta.token().empty())
        throw Exception{gap_internal_error, "you must be logged in"};

      return this->_meta;
    }

    /*----------------------.
    | Login/Logout/Register |
    `----------------------*/
    void
    State::login(std::string const& email,
                 std::string const& password)
    {
      ELLE_TRACE_SCOPE("%s: login to meta as %s", *this, email);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      reactor::Scheduler& scheduler = *reactor::Scheduler::scheduler();

      reactor::Lock l(this->_login_mutex);

      if (this->logged_in())
        throw Exception(
          gap_already_logged_in,
          elle::sprintf("already logged in as %s", this->meta().email()));

      this->_meta.token("");
      this->_cleanup();

      std::string lower_email = email;

      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      elle::SafeFinally login_failed{[this, lower_email] {
        this->_reporter[lower_email].store("user.login.failed");
        this->_google_reporter[lower_email].store("user.login.failed");
      }};

      auto res = this->_meta.login(lower_email, password);
      login_failed.abort();

      ELLE_LOG("Logged in as %s token = %s", email, res.token);
      this->_reporter[res.id].store(
          "user.login",
          {{MKey::session, "start"}, {MKey::status, "succeed"}});
      this->_google_reporter[res.id].store(
        "user.login",
        {{MKey::session, "start"}, {MKey::status, "succeed"}});
      ELLE_LOG("id: '%s' - fullname: '%s' - lower_email: '%s'",
                 this->me().id,
                 this->me().fullname,
                 this->me().email);

      std::string identity_clear;

      papier::Identity identity;

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

      this->_trophonius.connect(
        this->me().id, this->_meta.token(), this->device().id);
      this->_polling_thread.reset(
        new reactor::Thread{
          scheduler,
          "poll",
          [&]
          {
            while (true)
            {
              reactor::Scheduler::scheduler()->current()->wait(
                this->_polling_barrier);
              this->handle_notification(this->_trophonius.poll());
              ELLE_TRACE("%s: notification pulled", *this);
            }
          }});
      this->user(this->me().id);
      this->_transactions_init();
      this->on_connection_changed(true);
    }

    void
    State::logout()
    {
      ELLE_TRACE_METHOD("");

      /// First step must be to disconnect from trophonius.
      /// If not, you can pull notification that
      if (this->_polling_thread != nullptr)
      {
        this->_polling_thread->terminate_now();
        this->_polling_thread.reset();
      }
      this->_trophonius.disconnect();

      this->_users.clear();
      this->_transactions_clear();

      if (this->meta(false).token().empty())
        return;

      elle::SafeFinally logout(
        [&]
        {
          try
          {
            auto id = this->me().id;

            this->_meta.logout();

            this->_reporter[id].store("user.logout");
            this->_google_reporter[id].store(
              "user.logout",
              {{MKey::session, "end"}});
          }
          catch (reactor::Terminate const&)
          {
            throw;
          }
          catch (std::exception const&)
          {
            ELLE_WARN("logout failed, ignore exception: %s",
                      elle::exception_string());
            this->_meta.token("");
          }
        });

      elle::With<elle::Finally> clean([&] { this->_cleanup(); });
    }

    void
    State::_cleanup()
    {
      ELLE_TRACE_SCOPE("%s: cleaning up the state", *this);

      this->_device.reset();
      this->_me.reset();
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

      std::string lower_email = email;

      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      // Logout first, and ignore errors.
      try { this->logout(); } catch (std::exception const&) {}

      elle::SafeFinally register_failed{[this, lower_email] {
        this->_reporter[lower_email].store("user.register.failed");
      }};

      auto res = this->meta(false).register_(
        lower_email, fullname, password, activation_code);

      register_failed.abort();

      ELLE_DEBUG("registered new user %s <%s>", fullname, lower_email);

      elle::SafeFinally registered_metric{[this, res] {
        this->_reporter[res.registered_user_id].store(
          "user.register",
          {{MKey::source, res.invitation_source}});
      }};
      this->login(lower_email, password);
    }

    std::string const&
    State::token_generation_key() const
    {
      ELLE_TRACE_SCOPE("%s: generate token", *this);

      return this->me().token_generation_key;
    }

    Self const&
    State::me() const
    {
      static reactor::Mutex me_mutex;

      reactor::Lock m(me_mutex);
      if (this->_me == nullptr)
      {
        ELLE_TRACE("loading self info")
          this->_me.reset(new Self{this->meta().self()});
      }
      ELLE_ASSERT_NEQ(this->_me, nullptr);
      return *this->_me;
    }

    std::string
    State::user_directory()
    {
      ELLE_TRACE_METHOD("");

      return common::infinit::user_directory(this->me().id);
    }

    void
    State::output_dir(std::string const& dir)
    {
      if (!fs::exists(dir))
        throw Exception{gap_error,
                        "directory doesn't exist."};

      if (!fs::is_directory(dir))
        throw Exception{gap_error,
                        "not a directroy."};

      this->_output_dir = dir;
    }

    void
    State::on_connection_changed(bool connection_status)
    {
      ELLE_TRACE_SCOPE(
        "%s: connection %s", *this, connection_status ? "established" : "lost");

      // Lock polling.
      if (!connection_status)
        this->_polling_barrier.close();

      if (connection_status)
      {
        bool resynched{false};
        do
        {
          try
          {
            this->_user_resync();
            this->_transaction_resync();
            resynched = true;
          }
          catch (reactor::Terminate const&)
          {
            throw;
          }
          catch (std::exception const&)
          {
            ELLE_WARN("%s: failed at resynching (%s)... retrying...",
                      *this, elle::exception_string());
            reactor::sleep(1_sec);
          }
        }
        while (!resynched);
      }

      this->enqueue<ConnectionStatus>(ConnectionStatus(connection_status));

      // Unlock polling.
      if (connection_status)
        this->_polling_barrier.open();
    }

    void
    State::handle_notification(
      std::unique_ptr<plasma::trophonius::Notification>&& notif)
    {
      ELLE_TRACE_SCOPE("%s: new notification %s", *this, *notif);
      switch(notif->notification_type)
      {
        case plasma::trophonius::NotificationType::user_status:
          ELLE_ASSERT(
            dynamic_cast<plasma::trophonius::UserStatusNotification const*>(
              notif.get()) != nullptr);
          this->_on_swagger_status_update(
            *static_cast<plasma::trophonius::UserStatusNotification const*>(
              notif.release()));
          break;
        case plasma::trophonius::NotificationType::transaction:
          ELLE_ASSERT(
            dynamic_cast<plasma::Transaction const*>(notif.get()) != nullptr);
          this->_on_transaction_update(
            *static_cast<plasma::trophonius::TransactionNotification const*>(notif.release()));
          break;
        case plasma::trophonius::NotificationType::new_swagger:
          ELLE_ASSERT(
            dynamic_cast<plasma::trophonius::NewSwaggerNotification const*>(
              notif.get()) != nullptr);
          this->_on_new_swagger(
            *static_cast<plasma::trophonius::NewSwaggerNotification const*>(
              notif.release()));
          break;
        case plasma::trophonius::NotificationType::peer_connection_update:
          ELLE_ASSERT(
            dynamic_cast<plasma::trophonius::PeerConnectionUpdateNotification const*>(
              notif.get()) != nullptr);
          this->_on_peer_connection_update(
            *static_cast<plasma::trophonius::PeerConnectionUpdateNotification const*>(
              notif.release()));
          break;
        case plasma::trophonius::NotificationType::none:
        case plasma::trophonius::NotificationType::network_update:
        case plasma::trophonius::NotificationType::message:
        case plasma::trophonius::NotificationType::ping:
        case plasma::trophonius::NotificationType::connection_enabled:
        case plasma::trophonius::NotificationType::suicide:
          break;
      }
    };

    /*-------------------.
    | External Callbacks |
    `-------------------*/

    void
    State::poll() const
    {
      ELLE_DUMP("poll");

      if (this->_runners.empty())
        return;

      ELLE_DEBUG("poll %s notification(s)", this->_runners.size());

      // I'm the only consumer.
      while (!this->_runners.empty())
      {
        ELLE_ASSERT(!this->_runners.empty());
        std::unique_ptr<_Runner> runner = nullptr;

        {
          std::lock_guard<std::mutex> lock{this->_poll_lock};
          std::swap(runner, this->_runners.front());
          this->_runners.pop();
        }

        (*runner)();
      }
    }

    /*----------.
    | Printable |
    `----------*/
    void
    State::print(std::ostream& stream) const
    {
      stream << "state(" << this->meta(false).host()
             << ":" << this->meta(false).port();
      if (!this->meta(false).email().empty())
        stream << " as " << this->meta(false).email();
      stream << ")";
    }

    std::ostream&
    operator <<(std::ostream& out,
                NotificationType const& t)
    {
      switch (t)
      {
        case NotificationType_NewTransaction:
          return out << "NewTransaction";
        case NotificationType_TransactionUpdate:
          return out << "TransactionUpdate";
        case NotificationType_UserStatusUpdate:
          return out << "UserStatusUpdate";
        case NotificationType_NewSwagger:
          return out << "NewSwagger";
        case NotificationType_ConnectionStatus:
          return out << "ConnectionStatus";
      }
      return out;
    }
  }
}
