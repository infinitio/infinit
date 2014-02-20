#include <fstream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <openssl/sha.h>

#include <elle/format/gzip.hh>
#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/os/environ.hh>
#include <elle/os/path.hh>
#include <elle/serialize/HexadecimalArchive.hh>

#include <reactor/http/exceptions.hh>

#include <common/common.hh>

#include <papier/Identity.hh>

#include <infinit/metrics/reporters/GoogleReporter.hh>
#include <infinit/metrics/reporters/InfinitReporter.hh>

#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/SendMachine.hh>
#include <surface/gap/State.hh>
#include <surface/gap/TransactionMachine.hh>
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
        if (elle::os::inenv("INFINIT_LOG_FILE_PID"))
        {
          log_file += ".";
          log_file += std::to_string(::getpid());
        }

        static std::ofstream out{
          log_file + ".log",
          std::fstream::trunc | std::fstream::out};

        static elle::format::gzip::Stream compressed(out, false, 1024);

        elle::log::logger(
          std::unique_ptr<elle::log::Logger>{new elle::log::TextLogger(
            compressed)});
      }
      ELLE_LOG("Infinit Version: %s", INFINIT_VERSION);
    }

    /*--------------.
    | Notifications |
    `--------------*/
    State::ConnectionStatus::ConnectionStatus(bool status):
      status(status)
    {}
    Notification::Type State::ConnectionStatus::type =
      NotificationType_ConnectionStatus;

    Notification::Type State::KickedOut::type = NotificationType_KickedOut;

    Notification::Type State::TrophoniusUnavailable::type =
      NotificationType_TrophoniusUnavailable;

    /*-------------------------.
    | Construction/Destruction |
    `-------------------------*/
    State::State(std::string const& meta_host,
                 uint16_t meta_port,
                 std::string const& trophonius_host,
                 uint16_t trophonius_port):
      _logger_intializer{},
      _meta{meta_host, meta_port},
      _meta_message{""},
      _trophonius{
        trophonius_host,
        trophonius_port,
        [this] (bool status)
          {
            this->on_connection_changed(status);
          },
        [this] (void)
          {
            this->on_reconnection_failed();
          }
      },
      _composite_reporter{},
      _me{nullptr},
      _output_dir{common::system::download_directory()},
      _device{nullptr}
    {
      ELLE_TRACE_SCOPE("%s: create state", *this);

      // Add metrics reporters to composite reporter
      std::unique_ptr<infinit::metrics::Reporter> infinit_reporter(
        new infinit::metrics::InfinitReporter());
      this->_composite_reporter.add_reporter(std::move(infinit_reporter));
      this->_composite_reporter.start();
    }

    State::~State()
    {
      ELLE_TRACE_SCOPE("%s: destroying state", *this);

      try
      {
        this->logout();
        this->_composite_reporter.stop();
      }
      catch (std::runtime_error const&)
      {
        ELLE_WARN("Couldn't logout: %s", elle::exception_string());
      }

      ELLE_TRACE("%s: destroying members", *this);
    }

    infinit::oracles::meta::Client const&
    State::meta(bool authentication_required) const
    {
      if (authentication_required && !this->_meta.logged_in())
        throw Exception{gap_not_logged_in, "you must be logged in"};

      return this->_meta;
    }

    /*---------------------------.
    | Server Connection Checking |
    `---------------------------*/
    bool
    State::_meta_server_check(reactor::Duration timeout)
    {
      ELLE_TRACE_SCOPE("%s: fetching Meta status", *this);
      bool result = false;
      return elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        scope.run_background("meta status check", [&]
        {
          try
          {
            auto meta_response = this->_meta.server_status();
            if (meta_response.status)
            {
              ELLE_LOG("%s: Meta is reachable", *this);
              result = true;
            }
            else
            {
              this->_meta_message = meta_response.message;
              ELLE_WARN("%s: Meta down with message: %s",
                        *this,
                        this->_meta_message);
              result = false;
            }
          }
          catch (reactor::http::RequestError const& e)
          {
            ELLE_WARN("%s: unable to contact Meta: %s",
                     *this,
                     e.what());
            result = false;
          }
          catch (elle::http::Exception const& e)
          {
            ELLE_WARN("%s: unable to contact Meta: %s",
                     *this,
                     e.what());
            result = false;
          }
          catch (reactor::network::Exception const& e)
          {
            ELLE_WARN("%s: unable to contact Meta: %s",
                     *this,
                     e.what());
            result = false;
          }
          catch (elle::Exception const& e)
          {
            ELLE_WARN("%s: error while checking meta connectivity: %s",
                     *this,
                     e.what());
            result = false;
            // XXX: We shouldn't be catching all exceptions but the old JSON
            // parser throws elle::Exceptions.
          }
        });
        scope.wait(timeout);
        return result;
      };
    }

    bool
    State::_meta_server_check()
    {
      return this->_meta_server_check(3_sec);
    }

    bool
    State::_trophonius_server_check()
    {
      ELLE_TRACE("%s: check trophonius availablity", *this);
      if (this->_trophonius.poke())
      {
        ELLE_LOG("%s: successfully poked Trophonius", *this);
        return true;
      }
      else
      {
        ELLE_ERR("%s: Trophonius poke failed", *this);
        return false;
      }
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

      // Ensure we don't have an old Meta message
      this->_meta_message.clear();

      if (this->logged_in())
        throw Exception(gap_already_logged_in, "already logged in");

      this->_cleanup();

      if (!this->_meta_server_check())
      {
        if (this->_meta_message.empty())
        {
          throw Exception(gap_meta_unreachable,
                          "Unable to contact Meta");
        }
        else
        {
          throw Exception(gap_meta_down_with_message,
                          elle::sprintf("Meta down with message: %s",
                                        this->_meta_message));
        }
      }
      if (!this->_trophonius_server_check())
      {
        throw Exception(gap_trophonius_unreachable,
                        "Unable to contact Trophonius");
      }

      std::string lower_email = email;
      std::string fail_reason = "";

      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      elle::SafeFinally login_failed{[this, lower_email, fail_reason] {
        infinit::metrics::Reporter::metric_sender_id(lower_email);
        this->_composite_reporter.user_login(false, fail_reason);
      }};

      boost::uuids::uuid device_uuid = boost::uuids::nil_generator()();
      if (boost::filesystem::exists(common::infinit::device_id_path()))
      {
        ELLE_TRACE("%s: get device uuid from file", *this);
        std::ifstream file(common::infinit::device_id_path());
        std::string struuid;
        file >> struuid;
        device_uuid = boost::uuids::string_generator()(struuid);
      }
      else
      {
        ELLE_TRACE("%s: create device uuid", *this);
        device_uuid = boost::uuids::random_generator()();
        std::ofstream file(common::infinit::device_id_path());
        file << device_uuid << std::endl;
      }

      ELLE_DEBUG("%s: device uuid %s", *this, device_uuid);

      auto res = this->_meta.login(lower_email, password, device_uuid);

      fail_reason = res.error_details;

      login_failed.abort();

      elle::With<elle::Finally>([&] { this->_meta.logout(); })
        << [&] (elle::Finally& finally_logout)
      {
        ELLE_LOG("Logged in as %s", email);

        infinit::metrics::Reporter::metric_sender_id(res.id);
        this->_composite_reporter.user_login(true, "");

        ELLE_LOG("id: '%s' - fullname: '%s' - lower_email: '%s'",
                 this->me().id,
                 this->me().fullname,
                 this->me().email);

        std::string identity_clear;

        this->_identity.reset(new papier::Identity());

        // Decrypt the identity.
        if (this->_identity->Restore(res.identity)    == elle::Status::Error ||
            this->_identity->Decrypt(password)        == elle::Status::Error ||
            this->_identity->Clear()                  == elle::Status::Error ||
            this->_identity->Save(identity_clear)     == elle::Status::Error)
          throw Exception(gap_internal_error,
                          "Couldn't decrypt the identity file !");

        // Store the identity
        {
          if (this->_identity->Restore(identity_clear) == elle::Status::Error)
            throw Exception(gap_internal_error,
                            "Cannot save the identity file.");
          auto user_id = this->_identity->id();
          elle::io::Path path(elle::os::path::join(
                                common::infinit::user_directory(user_id),
                                user_id + ".idy"));
          this->_identity->store(path);
        }

        std::ofstream identity_infos{common::infinit::identity_path(res.id)};

        if (identity_infos.good())
        {
          identity_infos << res.identity << "\n"
                         << res.email << "\n"
                         << res.id << "\n"
          ;
          identity_infos.close();
        }

        auto device = this->meta().device(device_uuid);
        this->_device.reset(new Device{device.id, device.name});
        std::string passport_path =
          common::infinit::passport_path(this->me().id);
        this->_passport.reset(new papier::Passport());
        if (this->_passport->Restore(device.passport) == elle::Status::Error)
          throw Exception(gap_wrong_passport, "Cannot load the passport");
        this->_passport->store(elle::io::Path(passport_path));

        this->_trophonius.connect(
          this->me().id, this->device().id, this->_meta.session_id());

        this->_polling_thread.reset(
          new reactor::Thread{
            scheduler,
              "poll",
              [&]
              {
                while (true)
                {
                  try
                  {
                    this->handle_notification(this->_trophonius.poll());
                  }
                  catch (elle::Exception const&)
                  {
                    ELLE_ERR("%s: an error occured in trophonius, login is " \
                             "required: %s", *this, elle::exception_string());
                    // Loging out flush the message queue, which means that
                    // KickedOut will be the next event polled.
                    this->logout();
                    this->enqueue(KickedOut());
                    this->enqueue(ConnectionStatus(false));
                    return;
                  }

                  ELLE_TRACE("%s: notification pulled", *this);
                }
              }});

        this->_avatar_fetcher_thread.reset(
          new reactor::Thread{
            scheduler,
            "avatar fetched",
            [&]
            {
              while (true)
              {
                this->_avatar_fetching_barrier.wait();

                ELLE_ASSERT(!this->_avatar_to_fetch.empty());
                auto user_id = *this->_avatar_to_fetch.begin();
                auto id = this->_user_indexes.at(this->user(user_id).id);
                try
                {
                  this->_avatars.insert(
                    std::make_pair(id,
                                   this->_meta.icon(user_id)));
                  this->_avatar_to_fetch.erase(user_id);
                  this->enqueue(AvatarAvailableNotification(id));
                }
                catch (elle::Exception const& e)
                {
                  ELLE_ERR("%s: unable to fetch %s avatar: %s", *this, user_id,
                           e.what());
                  // The UI will ask for the avatar again if it needs it, so
                  // remove the request from the queue if there's a problem.
                  this->_avatar_to_fetch.erase(user_id);
                }

                if (this->_avatar_to_fetch.empty())
                  this->_avatar_fetching_barrier.close();
              }
            }});

        this->user(this->me().id);
        this->_transactions_init();
        this->on_connection_changed(true);

        finally_logout.abort();
      };
    }

    void
    State::logout()
    {
      ELLE_TRACE_SCOPE("%s: logout", *this);

      this->_cleanup();

      ELLE_DEBUG("%s: cleaned up", *this);

      if (!this->logged_in())
      {
        ELLE_DEBUG("%s: state was not logged in", *this);
        return;
      }

      elle::SafeFinally logout(
        [&]
        {
          try
          {
            auto id = this->me().id;

            this->_meta.logout();

            this->_composite_reporter.user_logout(true, "");
          }
          catch (elle::Exception const&)
          {
            ELLE_WARN("logout failed, ignore exception: %s",
                      elle::exception_string());
            this->_meta.logged_in(false);
          }
          ELLE_TRACE("%s: logged out", *this);
        });
    }

    void
    State::_cleanup()
    {
      ELLE_TRACE_SCOPE("%s: cleaning up the state", *this);

      elle::SafeFinally clean_containers(
        [&]
        {
          ELLE_DEBUG("disconnect trophonius")
            this->_trophonius.disconnect();

          ELLE_DEBUG("remove pending callbacks")
            while (!this->_runners.empty())
              this->_runners.pop();

          ELLE_DEBUG("clear users")
          {
            this->_user_indexes.clear();
            this->_swagger_indexes.clear();
            this->_users.clear();

            this->_avatar_fetching_barrier.close();
            if (this->_avatar_fetcher_thread != nullptr)
            {
              ELLE_DEBUG("stop avatar_fetching");
              this->_avatar_fetcher_thread->terminate_now();
              this->_avatar_fetcher_thread.reset();
            }
            this->_avatar_to_fetch.clear();
            this->_avatars.clear();
          }

          ELLE_DEBUG("clear transactions")
            this->_transactions_clear();

          ELLE_DEBUG("clear device (%s)", this->_device.get())
            this->_device.reset();

          ELLE_DEBUG("clear me (%s)", this->_me.get())
            this->_me.reset();

          ELLE_DEBUG("clear passport (%s)", this->_passport.get())
            this->_passport.reset();

          ELLE_DEBUG("clear identity (%s)", this->_identity.get())
            this->_identity.reset();

          ELLE_TRACE("everything has been cleaned");
        });

      /// First step must be to disconnect from trophonius.
      /// If not, you can pull notification that
      if (this->_polling_thread != nullptr &&
          reactor::Scheduler::scheduler()->current() != this->_polling_thread.get())
      {
        ELLE_DEBUG("stop polling");
        this->_polling_thread->terminate_now();
        this->_polling_thread.reset();
      }
    }

    std::string
    State::hash_password(std::string const& email,
                         std::string const& password)
    {
      // !WARNING! Do not log the password.
      ELLE_TRACE_FUNCTION(email);

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

      std::string error_details = "";

      elle::SafeFinally register_failed{[this, lower_email, error_details] {
        infinit::metrics::Reporter::metric_sender_id(lower_email);
        this->_composite_reporter.user_register(false, error_details);
      }};

      auto res = this->meta(false).register_(
        lower_email, fullname, password, activation_code);

      error_details = res.error_details;

      register_failed.abort();

      ELLE_DEBUG("registered new user %s <%s>", fullname, lower_email);

      elle::SafeFinally registered_metric{[this, res] {
        infinit::metrics::Reporter::metric_sender_id(res.registered_user_id);
        this->_composite_reporter.user_register(true, "");
      }};
      this->login(lower_email, password);
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

    void
    State::set_avatar(boost::filesystem::path const& image_path)
    {
      if (!boost::filesystem::exists(image_path))
        throw Exception(gap_error,
                        elle::sprintf("file not found at %s", image_path));

      std::ifstream stream{image_path.string()};
      std::istream_iterator<char> eos;
      std::istream_iterator<char> iit(stream);   // stdin iterator

      elle::Buffer output;
      std::copy(iit, eos, output.begin());

      this->set_avatar(output);
    }

    void
    State::set_avatar(elle::Buffer const& avatar)
    {
      this->meta().icon(this->me().id, avatar);
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

      if (connection_status)
      {
        bool resynched{false};
        do
        {
          if (!this->logged_in())
            return;

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
          catch (elle::Exception const&)
          {
            ELLE_WARN("%s: failed at resynching (%s). Retrying...",
                      *this, elle::exception_string());
            reactor::sleep(1_sec);
          }
        }
        while (!resynched);
      }

      this->enqueue<ConnectionStatus>(ConnectionStatus(connection_status));
    }

    void
    State::on_reconnection_failed()
    {
      if (this->_meta_server_check())
      {
        ELLE_ERR("%s: able to connect to Meta but not Trophonius", *this);
        this->enqueue(TrophoniusUnavailable());
      }
    }

    void
    State::handle_notification(
      std::unique_ptr<infinit::oracles::trophonius::Notification>&& notif)
    {
      ELLE_TRACE_SCOPE("%s: new notification %s", *this, *notif);
      switch(notif->notification_type)
      {
        case infinit::oracles::trophonius::NotificationType::user_status:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::UserStatusNotification const*>(
              notif.get()) != nullptr);
          this->_on_swagger_status_update(
            *static_cast<infinit::oracles::trophonius::UserStatusNotification const*>(
              notif.release()));
          break;
        case infinit::oracles::trophonius::NotificationType::transaction:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::TransactionNotification const*>(notif.get()) != nullptr);
          this->_on_transaction_update(
            *static_cast<infinit::oracles::trophonius::TransactionNotification const*>(notif.release()));
          break;
        case infinit::oracles::trophonius::NotificationType::new_swagger:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::NewSwaggerNotification const*>(
              notif.get()) != nullptr);
          this->_on_new_swagger(
            *static_cast<infinit::oracles::trophonius::NewSwaggerNotification const*>(
              notif.release()));
          break;
        case infinit::oracles::trophonius::NotificationType::peer_connection_update:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::PeerConnectionUpdateNotification const*>(
              notif.get()) != nullptr);
          this->_on_peer_connection_update(
            *static_cast<infinit::oracles::trophonius::PeerConnectionUpdateNotification const*>(
              notif.release()));
          break;
        case infinit::oracles::trophonius::NotificationType::none:
        case infinit::oracles::trophonius::NotificationType::network_update:
        case infinit::oracles::trophonius::NotificationType::message:
        case infinit::oracles::trophonius::NotificationType::ping:
        case infinit::oracles::trophonius::NotificationType::connection_enabled:
        case infinit::oracles::trophonius::NotificationType::suicide:
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
        case NotificationType_KickedOut:
          return out << "Kicked Out";
        case NotificationType_AvatarAvailable:
          return out << "Avatar Available";
        case NotificationType_TrophoniusUnavailable:
          return out << "Trophonius Unavailable";
      }

      return out;
    }
  }
}
