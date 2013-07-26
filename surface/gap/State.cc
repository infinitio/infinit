#include "State.hh"

#include <surface/gap/TransferMachine.hh>
#include <surface/gap/SendMachine.hh>
#include <surface/gap/ReceiveMachine.hh>

#include <common/common.hh>

#include <lune/Identity.hh>
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

    namespace
    {
      enum class MetricKind
      {
        user,
        network,
        transaction
      };

      template <common::metrics::Kind kind, typename Service>
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
          case common::metrics::Kind::all:
            return "";
          case common::metrics::Kind::user:
            return "user";
          case common::metrics::Kind::network:
            return "network";
          case common::metrics::Kind::transaction:
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
    // - State ----------------------------------------------------------------
    State::State(std::string const& meta_host,
                 uint16_t meta_port,
                 std::string const& trophonius_host,
                 uint16_t trophonius_port,
                 std::string const& apertus_host,
                 uint16_t apertus_port):
      _logger_intializer{},
      _meta{meta_host, meta_port, true},
      _reporter(),
      _google_reporter(),
      _me{nullptr},
      _device{nullptr},
      _trophonius_host{trophonius_host},
      _trophonius_port{trophonius_port},
      _apertus_host{apertus_host},
      _apertus_port{apertus_port}
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

      {
        using metrics::services::Google;
        using metrics::services::KISSmetrics;
        using namespace common::metrics;

        this->_google_reporter.add_service_class<Google>(google_info_investors());

        this->_reporter.add_service_class<Google>(google_info());
        this->_reporter.add_service_class<KISSmetrics>(kissmetrics_info());

        typedef MetricKindService<Kind::user, KISSmetrics> KMUser;
        this->_reporter.add_service_class<KMUser>(
          kissmetrics_info(Kind::user));

        typedef MetricKindService<Kind::network, KISSmetrics> KMNetwork;
        this->_reporter.add_service_class<KMNetwork>(
          kissmetrics_info(Kind::network));

        typedef MetricKindService<Kind::transaction, KISSmetrics> KMTransaction;
        this->_reporter.add_service_class<KMTransaction>(
          kissmetrics_info(Kind::transaction));
      }
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

      try
      {
        this->logout();
      }
      catch (std::runtime_error const&)
      {
        ELLE_WARN("Couldn't logout: %s", elle::exception_string());
      }
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
      ELLE_TRACE_SCOPE("%s: login to meta as %s", *this, email);

      this->_meta.token("");
      this->_cleanup();

      std::string lower_email = email;

      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      elle::Finally login_failed{[this, lower_email] {
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

      this->notification_manager();
      this->user_manager();
      this->network_manager();
      this->transaction_manager();

      // XXX: Will create the notification mananger :/
      this->notification_manager().transaction_callback(
        std::bind(&surface::gap::State::_on_transaction_notification,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2));

      this->notification_manager().peer_connection_update_callback(
        std::bind(&surface::gap::State::_on_peer_connection_update_notification,
                  this,
                  std::placeholders::_1));

      this->_init_transactions();

      this->notification_manager().connect();
    }

    void
    State::_init_transactions()
    {
      for (auto& transaction_pair: this->transaction_manager().all())
      {
        if (transaction_pair.second.sender_id == this->me().id)
        {
          //this->_transfers.emplace_back(new SendMachine{*this, recipient, {}});
        }
        else
        {
          this->_transfers.emplace_back(new ReceiveMachine{*this, transaction_pair.first});
        }
      }
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

      elle::Finally logout([&] {
          try
          {
            auto id = this->me().id;

            this->_meta.logout();

            this->_reporter[id].store("user.logout");
            this->_google_reporter[id].store(
              "user.logout",
              {{MKey::session, "end"}});
          }
          catch (std::exception const&)
          {
            ELLE_WARN("logout failed, ignore exception: %s",
                      elle::exception_string());
            this->_meta.token("");
          }
        });
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

      elle::Finally register_failed{[this, lower_email] {
        this->_reporter[lower_email].store("user.register.failed");
      }};

      auto res = this->_meta.register_(
        lower_email, fullname, password, activation_code);

      register_failed.abort();


      ELLE_DEBUG("Registered new user %s <%s>", fullname, lower_email);

      elle::Finally registered_metric{[this, res] {
        this->_reporter[res.registered_user_id].store(
          "user.register",
          {{MKey::source, res.invitation_source}});
      }};
      this->login(lower_email, password);
    }

    lune::Identity const&
    State::identity() const
    {
      if (!this->logged_in())
        throw Exception{gap_internal_error, "you must be logged in"};

      if (this->_identity.Restore(this->meta().identity()) == elle::Status::Error)
        throw elle::Exception("Couldn't restore the identity.");

      return this->_identity;
    }

    NotificationManager&
    State::notification_manager(bool auto_connect) const
    {
      return this->_notification_manager(
        [this, auto_connect] (NotificationManagerPtr& manager) -> NotificationManager& {
          if (manager == nullptr)
          {
            ELLE_TRACE_SCOPE("%s: allocating a new notification manager", *this);

            manager.reset(
              new NotificationManager{
                this->_trophonius_host,
                this->_trophonius_port,
                this->_meta,
                std::bind(&State::me, this),
                std::bind(&State::device, this),
                auto_connect,
              });
          }
          return *manager;
        });
    }

    NetworkManager&
    State::network_manager() const
    {
      return this->_network_manager(
        [this] (NetworkManagerPtr& manager) -> NetworkManager& {
          if (manager == nullptr)
          {
            ELLE_TRACE_SCOPE("%s: allocating a new network manager", *this);

            manager.reset(
              new NetworkManager{
                this->notification_manager(),
                this->_meta,
              });
          }
          return *manager;
        });
    }

    UserManager&
    State::user_manager() const
    {
      return this->_user_manager(
        [this] (UserManagerPtr& manager) -> UserManager& {
          if (manager == nullptr)
          {
            ELLE_TRACE_SCOPE("%s: allocating a new user manager", *this);

            manager.reset(
              new UserManager{
                this->notification_manager(),
                this->_meta
              });
          }
          return *manager;
        });
    }

    TransactionManager&
    State::transaction_manager() const
    {
      return this->_transaction_manager(
        [this] (TransactionManagerPtr& manager) -> TransactionManager& {
          if (manager == nullptr)
          {
            ELLE_TRACE_SCOPE("%s: allocating a new transaction manager", *this);
            // auto update_remaining_invitations =
            //   [this] (unsigned int remaining_invitations)
            //   {
            //     if (this->_me != nullptr)
            //       this->_me->remaining_invitations = remaining_invitations;
            //   };
            manager.reset(
              new TransactionManager{
                this->notification_manager(),
                this->meta(),
                std::bind(&State::me, this),
                std::bind(&State::device, this)
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

    TransferMachine&
    State::_find_machine(std::function<bool (TransferMachinePtr const&)> func) const
    {
      auto it =
        std::find_if(std::begin(this->_transfers),
                     std::end(this->_transfers),
                     func);

      if (it != std::end(this->_transfers))
      {
        return *(*it);
      }
      else
      {
        throw Exception(gap_error, "machine doesnt exist");
      }
    }

    TransferMachine&
    State::_machine_by_user(std::string const& user_id) const
    {
      return this->_find_machine(
        [&] (TransferMachinePtr const& machine)
        {
          auto const& peers = machine->peers(); // under-optimized.
          return std::find(std::begin(peers), std::end(peers), user_id) != std::end(peers);
        });
    }

    TransferMachine&
    State::_machine_by_transaction(std::string const& transaction_id) const
    {
      return this->_find_machine(
        [&] (TransferMachinePtr const& machine)
        {
          return machine->transaction_id() == transaction_id;
        });
    }

    TransferMachine&
    State::_machine_by_network(std::string const& network_id) const
    {
      return this->_find_machine(
        [&] (TransferMachinePtr const& machine)
        {
          return machine->network_id() == network_id;
        });
    }

    void
    State::_on_user_notification(UserStatusNotification const& notif)
    {
    }

    void
    State::_on_network_notification(NetworkUpdateNotification const& notif)
    {
    }

    void
    State::_on_transaction_notification(TransactionNotification const& notif,
                                        bool)
    {
      ELLE_TRACE_SCOPE("%s: transaction_notification %s", *this, notif);
      auto const& transaction = this->transaction_manager().one(notif.id);

      try
      {
        auto& transfer_machine = this->_machine_by_transaction(notif.id);
        transfer_machine.on_transaction_update(transaction);
      }
      catch (Exception const& e)
      {
        if (e.code == gap_error) // XXX:
        {
          if (notif.status == plasma::TransactionStatus::initialized)
            this->_transfers.emplace_back(new ReceiveMachine{*this, notif.id});
          else
            ELLE_ERR("%s: machine not found for transaction %s",
                     *this, transaction);
          return;
        }
        else
          throw;
      }
    }

    void
    State::send_files(std::string const& recipient,
                      std::unordered_set<std::string>&& files)
    {
      ELLE_TRACE_SCOPE("%s: send file %s to %s", *this, files, recipient);
      this->_transfers.emplace_back(
        new SendMachine{*this, recipient, std::move(files)});
    }

    void
    State::accept_transaction(std::string const& transaction_id)
    {
      ELLE_TRACE_SCOPE("%s: accept transaction %s", *this, transaction_id);

      try
      {
        auto& transfer_machine = this->_machine_by_transaction(transaction_id);

        if (!transfer_machine.is_sender(this->me().id))
        {
          auto& receive_machine = (ReceiveMachine&) transfer_machine;
          receive_machine.accept();
        }
      }
      catch (Exception const&)
      {
        throw;
      }
    }

    void
    State::cancel_transaction(std::string const& transaction_id) {}

    void
    State::reject_transaction(std::string const& transaction_id) {}

  }
}
