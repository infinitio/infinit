#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <elle/AtomicFile.hh>
#include <elle/cast.hh>
#include <elle/format/gzip.hh>
#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/os/environ.hh>
#include <elle/os/path.hh>
#include <elle/serialization/json/SerializerIn.hh>
#include <elle/serialization/json/SerializerOut.hh>
#include <elle/serialize/HexadecimalArchive.hh>
#include <elle/system/platform.hh>

#include <das/serializer.hh>

#include <reactor/Scope.hh>
#include <reactor/duration.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>

#include <common/common.hh>

#include <papier/Identity.hh>
#include <papier/Passport.hh>
#include <papier/Authority.hh>

#include <infinit/metrics/Reporter.hh>
#include <infinit/oracles/trophonius/Client.hh>

#include <surface/gap/Error.hh>
#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/SendMachine.hh>
#include <surface/gap/State.hh>
#include <surface/gap/TransactionMachine.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

DAS_MODEL_FIELD(surface::gap::State::GhostCode, code);
DAS_MODEL_FIELD(surface::gap::State::GhostCode, was_link);
typedef das::Object<
  surface::gap::State::GhostCode,
  das::Field<surface::gap::State::GhostCode, std::string, &surface::gap::State::GhostCode::code>,
  das::Field<surface::gap::State::GhostCode, bool, &surface::gap::State::GhostCode::was_link>
  >
DasGhostCode;
DAS_MODEL_DEFAULT(surface::gap::State::GhostCode, DasGhostCode);

namespace elle
{
  namespace serialization
  {
    template <>
    struct Serialize<surface::gap::State::GhostCode>
    {
      typedef das::Serializer<surface::gap::State::GhostCode> Wrapper;
    };
  }
}

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

        this->_output.reset(
          new std::ofstream{
            log_file,
              std::fstream::trunc | std::fstream::out});

        std::string log_level =
          "elle.CrashReporter:DEBUG,"
          "*FIST*:TRACE,"
          "*FIST.State*:DEBUG,"
          "frete.Frete:TRACE,"
          "infinit.surface.gap.Rounds:DEBUG,"
          "*meta*:TRACE,"
          "Gap-ObjC++.*:DEBUG,"
          "iOS.*:DEBUG,"
          "OSX*:DEBUG,"
          "reactor.fsm.*:TRACE,"
          "reactor.network.upnp:DEBUG,"
          "station.Station:DEBUG,"
          "*surface.gap.*:TRACE,"
          "surface.gap.*Machine:DEBUG,"
          "*trophonius*:DEBUG";
        bool display_type = true;
        bool enable_pid = false;
        bool enable_tid = true;
        bool enable_time = true;
        bool universal_time = false;

        auto logger_ptr = std::unique_ptr<elle::log::Logger>(
          new elle::log::TextLogger(*this->_output,
                                    log_level,
                                    display_type,
                                    enable_pid,
                                    enable_tid,
                                    enable_time,
                                    universal_time));
        elle::log::logger(std::move(logger_ptr));
      }
      ELLE_LOG("Infinit Version: %s running on %s", INFINIT_VERSION,
               elle::system::platform::os_description());
    }

    LoggerInitializer::~LoggerInitializer()
    {
      std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");
      if (!log_file.empty())
      {
        elle::log::logger(
          std::unique_ptr<elle::log::Logger>{
            new elle::log::TextLogger(std::cerr)});

        this->_output.reset();
      }
    }

    /*--------------.
    | Notifications |
    `--------------*/

    State::ConnectionStatus::ConnectionStatus(bool status,
                                              bool still_trying,
                                              std::string const& last_error)
      : status(status)
      , still_trying(still_trying)
      , last_error(last_error)
    {}
    Notification::Type State::ConnectionStatus::type =
      NotificationType_ConnectionStatus;

    /*-------------------------.
    | Construction/Destruction |
    `-------------------------*/

    State::State(common::infinit::Configuration local_config)
      : _logger_intializer()
      , _meta(local_config.meta_protocol(),
              local_config.meta_host(),
              local_config.meta_port())
      , _meta_message("")
      , _trophonius_fingerprint(local_config.trophonius_fingerprint())
      , _trophonius(nullptr)
      , _forced_trophonius_host()
      , _forced_trophonius_port(0)
      , _metrics_reporter(std::move(local_config.metrics()))
      , _local_configuration(local_config)
      , _me()
      , _output_dir(local_config.download_dir())
      , _reconnection_cooldown(10_sec)
      , _device_uuid(std::move(local_config.device_id()))
      , _device()
      , _referral_code()
      , _login_watcher_thread(nullptr)
      , _authority(local_config.authority())
    {
      this->_logged_out.open();
      ELLE_TRACE_SCOPE("%s: create state", *this);
      if (!this->_metrics_reporter)
        // This is a no-op reporter.
        this->_metrics_reporter.reset(new infinit::metrics::CompositeReporter);
      this->_metrics_reporter->start();
      infinit::metrics::Reporter::metric_device_id(
        boost::lexical_cast<std::string>(this->device_uuid()));
      ELLE_LOG("%s: device uuid: %s", *this, this->device_uuid());
      // Fill configuration.
      auto& config = this->_configuration;
#if defined(INFINIT_ANDROID) || defined(INFINIT_IOS)
      config.s3.multipart_upload.parallelism = 2;
#else
      config.s3.multipart_upload.parallelism = 8;
#endif
      config.s3.multipart_upload.chunk_size = 0;
      config.enable_file_mirroring =
        this->local_configuration().enable_mirroring();
      config.max_mirror_size = this->local_configuration().max_mirror_size();
      config.max_compress_size = 0;
      config.max_cloud_buffer_size = 0;
      config.disable_upnp = false;
      ELLE_TRACE("read local config")
      {
        std::ifstream fconfig(this->local_configuration().configuration_path());
        if (fconfig.good())
        {
          try
          {
            elle::json::Object obj = boost::any_cast<elle::json::Object>(
              elle::json::read(fconfig));
            this->_apply_configuration(obj);
          }
          catch(std::exception const& e)
          {
            ELLE_ERR("%s: while reading configuration: %s", *this, e.what());
            std::stringstream str;
            {
              elle::serialization::json::SerializerOut output(str, false);
              this->_configuration.serialize(output);
            }
            ELLE_TRACE("%s: current config: %s", *this, str.str());
          }
        }
      }
      this->_check_first_launch();
      this->_check_forced_trophonius();
    }

    std::string
    State::session_id() const
    {
      return this->_meta.session_id();
    }

    void
    State::_kick_out(bool retry,
                     std::string const& message)
    {
      ELLE_TRACE_SCOPE("kicked out: %s (retry: %s)", message, retry);
      this->logout();
      this->enqueue(ConnectionStatus(false, retry, message));
    }

    State::State(std::string const& meta_protocol,
                 std::string const& meta_host,
                 uint16_t meta_port,
                 std::vector<unsigned char> trophonius_fingerprint,
                 boost::optional<boost::uuids::uuid const&> device_id,
                 boost::optional<std::string> download_dir,
                 boost::optional<std::string> home_dir,
                 boost::optional<papier::Authority> authority)
      : State(common::infinit::Configuration(
                meta_protocol, meta_host, meta_port,
                trophonius_fingerprint,
                device_id,
                download_dir,
                home_dir,
                authority))
    {}

    State::~State()
    {
      ELLE_TRACE_SCOPE("%s: destroying state", *this);

      try
      {
        // We do this to ensure that Trophonius is properly killed. Trophonius
        // cannot be reset inside the _clean_up or logout functions as these
        // can be run by Trophonius's threads.
        this->_cleanup();
        this->_trophonius.reset();
        this->logout();
        this->_metrics_reporter->stop();
        if (this->_login_thread)
          this->_login_thread->terminate_now();
      }
      catch (std::runtime_error const&)
      {
        ELLE_WARN("Couldn't logout: %s", elle::exception_string());
      }

      ELLE_TRACE("%s: destroying members", *this);
    }

    void
    State::internet_connection(bool connected)
    {
      if (connected)
      {
        ELLE_TRACE("%s: got internet connection", *this);
      }
      else
      {
        ELLE_TRACE("%s: lost internet connection", *this);
      }
    }

    void
    State::set_proxy(reactor::network::Proxy const& proxy)
    {
      ELLE_TRACE("%s: set proxy: %s", *this, proxy);
      using ProxyType = reactor::network::ProxyType;
      switch (proxy.type())
      {
        case ProxyType::None:
          break;
        case ProxyType::HTTP:
          this->_set_http_proxy(proxy);
          break;
        case ProxyType::HTTPS:
          this->_set_https_proxy(proxy);
          break;
        case ProxyType::SOCKS:
          this->_set_socks_proxy(proxy);
          break;
      }
    }

    void
    State::unset_proxy(reactor::network::ProxyType const& proxy_type)
    {
      ELLE_TRACE("%s: unset proxy: %s", *this, proxy_type);
      using ProxyType = reactor::network::ProxyType;
      reactor::network::Proxy proxy;
      switch (proxy_type)
      {
        case ProxyType::None:
          break;
        case ProxyType::HTTP:
          this->_set_http_proxy(proxy);
          break;
        case ProxyType::HTTPS:
          this->_set_https_proxy(proxy);
          break;
        case ProxyType::SOCKS:
          this->_set_socks_proxy(proxy);
          break;
      }
    }

    void
    State::_set_http_proxy(reactor::network::Proxy const& proxy)
    {
      // The == operator of Proxy has been overriden to check only the type
      // and host of given proxies.
      bool same = this->_http_proxy == proxy;
      this->_http_proxy = proxy;
      if (this->_meta.protocol() == "http")
        this->_meta.default_configuration().proxy(proxy);
      this->_metrics_reporter->proxy(proxy);
      if (!same && !proxy.host().empty())
        this->_metrics_reporter->user_proxy(proxy.type());
    }

    void
    State::_set_https_proxy(reactor::network::Proxy const& proxy)
    {
      // The == operator of Proxy has been overriden to check only the type
      // and host of given proxies.
      bool same = this->_https_proxy == proxy;
      this->_https_proxy = proxy;
      if (this->_meta.protocol() == "https")
        this->_meta.default_configuration().proxy(proxy);
      if (!same && !proxy.host().empty())
        this->_metrics_reporter->user_proxy(proxy.type());
    }

    void
    State::_set_socks_proxy(reactor::network::Proxy const& proxy)
    {
      // The == operator of Proxy has been overriden to check only the type
      // and host of given proxies.
      bool same = this->_socks_proxy == proxy;
      this->_socks_proxy = proxy;
      if (!same && !proxy.host().empty())
        this->_metrics_reporter->user_proxy(proxy.type());
    }

    infinit::oracles::meta::Client const&
    State::meta(bool authentication_required) const
    {
      if (authentication_required && !this->logged_in_to_meta())
        throw Exception{gap_not_logged_in, "you must be logged in"};

      return this->_meta;
    }

    bool
    State::logged_in_to_meta() const
    {
      return this->_meta.logged_in();
    }

    void
    State::_check_first_launch()
    {
      ELLE_TRACE_SCOPE("%s: check if first launch", *this);
      namespace filesystem = boost::filesystem;
      filesystem::path first_launch_witness(
        this->local_configuration().first_launch_path());
      if (filesystem::exists(first_launch_witness))
      {
        ELLE_DEBUG("first launch witness found");
        return;
      }
      ELLE_TRACE("no first launch witness at %s", first_launch_witness);
      if (!filesystem::exists(first_launch_witness.parent_path()))
      {
        ELLE_TRACE("create directories %s", first_launch_witness.parent_path())
          filesystem::create_directories(first_launch_witness.parent_path());
      }
      ELLE_LOG("write first launch witness at %s", first_launch_witness);
      {
        elle::AtomicFile f(first_launch_witness);
        f.write() << [] (elle::AtomicFile::Write& write)
        {
          write.stream() << "0\n";
        };
      }
      ELLE_TRACE("report first launch metric")
        this->_metrics_reporter->user_first_launch();
    }

    /*----------------------.
    | Login/Logout/Register |
    `----------------------*/

    void
    State::connect()
    {
      if (this->_trophonius->connected().opened())
        return;
      ELLE_TRACE("%s: reconnecting to trophonius", *this);
      this->_trophonius->connect(
        this->me().id, this->device().id, this->_meta.session_id());
      reactor::wait(_trophonius->connected());
            ELLE_TRACE("%s: connected to trophonius", *this);
      ELLE_TRACE("%s: reconnected to trophonius", *this);
      this->_synchronize_response.reset(
        new infinit::oracles::meta::SynchronizeResponse{
        this->meta().synchronize(false)});
      this->_account(this->_synchronize_response->account);
      this->_devices(this->_synchronize_response->devices);
      this->_external_accounts(this->_synchronize_response->external_accounts);
      this->_user_resync(this->_synchronize_response->swaggers, false);
      this->_peer_transaction_resync(
        this->_synchronize_response->transactions, false);
      this->_link_transaction_resync(
        this->_synchronize_response->links, false);
    }

    std::string
    State::web_login_token()
    {
      return this->meta().web_login_token().token();
    }

    void
    State::login(
      std::string const& email,
      std::string const& password,
      boost::optional<std::string> device_push_token,
      boost::optional<std::string> country_code,
      boost::optional<std::string> device_model,
      boost::optional<std::string> device_name,
      boost::optional<std::string> device_language)
    {
      this->login(email,
                  password,
                  std::unique_ptr<infinit::oracles::trophonius::Client>(),
                  reactor::DurationOpt(),
                  device_push_token,
                  country_code,
                  device_model,
                  device_name,
                  device_language);
    }

    void
    State::login(
      std::string const& email,
      std::string const& password,
      reactor::DurationOpt timeout,
      boost::optional<std::string> device_push_token,
      boost::optional<std::string> country_code,
      boost::optional<std::string> device_model,
      boost::optional<std::string> device_name,
      boost::optional<std::string> device_language)
    {
      this->login(
        email, password,
        std::unique_ptr<infinit::oracles::trophonius::Client>(),
        timeout,
        device_push_token,
        country_code,
        device_model,
        device_name,
        device_language);
    }

    void
    State::_check_forced_trophonius()
    {
      std::string host_str = elle::os::getenv("INFINIT_TROPHONIUS_HOST", "");
      std::string port_str = elle::os::getenv("INFINIT_TROPHONIUS_PORT", "");
      if (!host_str.empty())
        this->_forced_trophonius_host = host_str;
      if (!port_str.empty())
        this->_forced_trophonius_port = boost::lexical_cast<int>(port_str);
    }

    void
    State::_login_with_timeout(std::function<void ()> login_function,
                               reactor::DurationOpt timeout)
    {
      if (this->logged_in_to_meta())
        this->logout();
      this->_logged_out.close();
      this-> _login_thread.reset(new reactor::Thread(
        "login",
        [=]
        {
          login_function();
        }));
      this->_login_watcher_thread = reactor::scheduler().current();
      elle::SafeFinally reset_login_thread(
        [&]
        {
          this->_login_watcher_thread = nullptr;
        });
      //Wait for logged-in or logged-out status
      elle::With<reactor::Scope>() << [&] (reactor::Scope& s)
      {
         s.run_background("waiter1",
          [&]
          {
            reactor::wait(this->_logged_in); s.terminate_now();
          });
         s.run_background("waiter2",
          [&]
          {
            reactor::wait(this->_logged_out); s.terminate_now();
          });
         reactor::wait(s, timeout);
         s.terminate_now();
      };

      ELLE_TRACE("Login user thread unblocked");
      if (!this->logged_in_to_meta())
        throw elle::Error("Login failure");
    }

    void
    State::login(
      std::string const& email,
      std::string const& password,
      TrophoniusClientPtr trophonius,
      reactor::DurationOpt timeout,
      boost::optional<std::string> device_push_token,
      boost::optional<std::string> country_code,
      boost::optional<std::string> device_model,
      boost::optional<std::string> device_name,
      boost::optional<std::string> device_language)
    {
      auto tropho = elle::utility::move_on_copy(std::move(trophonius));
      return this->_login_with_timeout(
        [&]
        {
          this->_login(
            email, password, tropho, device_push_token, country_code,
            device_model, device_name, device_language);
        }, timeout);
    }

    typedef infinit::oracles::trophonius::Client TrophoniusClient;
    typedef infinit::oracles::trophonius::ConnectionState ConnectionState;
    void
    State::_login(
      std::string const& email,
      std::string const& password,
      elle::utility::Move<TrophoniusClientPtr> trophonius,
      boost::optional<std::string> device_push_token,
      boost::optional<std::string> country_code,
      boost::optional<std::string> device_model,
      boost::optional<std::string> device_name,
      boost::optional<std::string> device_language)
    {
      ELLE_TRACE_SCOPE("%s: attempt to login as %s", *this, email);
      this->_email = email;
      this->_password = password;

      std::string lower_email = email;
      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);
      auto hashed_password = infinit::oracles::meta::old_password_hash(
        lower_email, password);
      bool res = this->_login(
        [&] {
          return this->_meta.login(lower_email,
                                   password,
                                   this->_device_uuid,
                                   device_push_token,
                                   country_code,
                                   device_model,
                                   device_name,
                                   device_language);
        },
        trophonius,
        [=] { return hashed_password; },
        [&] (bool success, std::string const& failure_reason) {
          infinit::metrics::Reporter::metric_sender_id(lower_email);
          this->_metrics_reporter->user_login(success, failure_reason);
        });
      if (res)
        this->_metrics_reporter->user_login(true, "");
    }

    bool
    State::_login(
      std::function<infinit::oracles::meta::LoginResponse ()> login_function,
      elle::utility::Move<std::unique_ptr<TrophoniusClient>> trophonius,
      std::function<std::string ()> identity_password,
      LoginMetric metric)
    {
      while(true) try
      {
        ELLE_TRACE_SCOPE("%s: login to meta", *this);
        ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
        reactor::Scheduler& scheduler = *reactor::Scheduler::scheduler();
        reactor::Lock l(this->_login_mutex);
        // Ensure we don't have an old Meta message
        this->_meta_message.clear();
        if (this->logged_in_to_meta())
          throw Exception(gap_already_logged_in, "already logged in");
        this->_cleanup();
        std::string failure_reason;
        elle::With<elle::Finally>([&]
          {
            failure_reason = elle::exception_string();
            ELLE_WARN("%s: error during login, logout: %s",
                      *this, failure_reason);
            metric(false, failure_reason);
            if (this->logged_in_to_meta())
              this->_meta.logout();
          })
          << [&] (elle::Finally& finally_logout)
        {
          auto login_response = login_function();
          ELLE_TRACE("%s: logged in to meta", *this);
          this->_me.reset(new Self(login_response.self));
          // Don't send notification for me user here.
          this->user_sync(this->me(), false);
          this->_configuration.features = login_response.features;
          infinit::metrics::Reporter::metric_features(
            this->_configuration.features);
          // Trophonius.
          if (*trophonius)
          {
            this->_trophonius.swap(*trophonius);
          }
          else if (!this->_trophonius)
          {
            this->_trophonius.reset(
              new infinit::oracles::trophonius::Client(
                [this]
                (infinit::oracles::trophonius::ConnectionState const& status)
                {
                  this->on_connection_changed(status);
                },
                std::bind(&State::on_reconnection_failed, this),
                this->_trophonius_fingerprint,
                this->_reconnection_cooldown
                )
              );
          }
          std::string trophonius_host = login_response.trophonius.host;
          int trophonius_port = login_response.trophonius.port_ssl;
          if (!this->_forced_trophonius_host.empty())
            trophonius_host = this->_forced_trophonius_host;
          if (this->_forced_trophonius_port != 0)
            trophonius_port = this->_forced_trophonius_port;
          this->_trophonius->server(trophonius_host, trophonius_port);

          // Update features before sending any metric
          this->_metrics_heartbeat_thread.reset(
            new reactor::Thread{
              *reactor::Scheduler::scheduler(),
                "metrics heartbeat",
                [&]
                {
                  while (true)
                  {
                    reactor::sleep(360_min);
                    this->_metrics_reporter->user_heartbeat();
                  }
                }});
          infinit::metrics::Reporter::metric_sender_id(this->me().id);

          // Identity.
          std::string identity_clear;
          ELLE_TRACE("%s: decrypt identity", *this)
          {
            this->_identity.reset(new papier::Identity());
            if (this->_identity->Restore(this->me().identity) == elle::Status::Error)
              throw Exception(gap_internal_error, "unable to restore the identity");
            if (this->_identity->Decrypt(identity_password()) == elle::Status::Error)
              throw Exception(gap_internal_error, "unable to decrypt the identity");
          }

          std::ofstream identity_infos{
            this->local_configuration().identity_path(this->me().id)};
          if (identity_infos.good())
          {
            identity_infos << this->me().identity << "\n"
                           << this->me().email << "\n"
                           << this->me().id << "\n"
            ;
            identity_infos.close();
          }

          // Device.
          this->_device.reset(new Device(login_response.device));
          ELLE_ASSERT_EQ(this->_device_uuid, this->_device->id);
          std::string passport_path =
            this->local_configuration().passport_path();
          this->_passport.reset(new papier::Passport());
          if (this->_passport->Restore(this->_device->passport.get()) ==
              elle::Status::Error)
            throw Exception(gap_wrong_passport, "Cannot load the passport");
          this->_passport->store(elle::io::Path(passport_path));

          ELLE_TRACE("%s: connecting to trophonius on %s:%s",
                     *this,
                     login_response.trophonius.host,
                     login_response.trophonius.port_ssl)
          {
            this->_trophonius->connect(
              this->me().id, this->device().id, this->_meta.session_id());
            reactor::wait(_trophonius->connected());
            ELLE_TRACE("%s: connected to trophonius", *this);
          }

          // Synchronization.
          this->_synchronize_response.reset(
            new infinit::oracles::meta::SynchronizeResponse{
              this->meta().synchronize(true)});
          ELLE_TRACE("got synchronisation response");
          this->_account(this->_synchronize_response->account);
          this->_devices(this->_synchronize_response->devices);
          this->_external_accounts(
            this->_synchronize_response->external_accounts);
          this->_model.devices.added().connect(
            [this] (Device& d)
            {
              this->_on_device_added(d);
            });
          this->_model.devices.removed().connect(
            [this] (elle::UUID id)
            {
              this->_on_device_removed(id);
            });
          this->_model.devices.reset().connect(
            [this] ()
            {
              this->_on_devices_reseted();
            });
          this->_avatar_fetcher_thread.reset(
            new reactor::Thread{
              scheduler,
              "avatar fetched",
              [&]
              {
                this->_logged_in.wait();
                while (true)
                {
                  this->_avatar_fetching_barrier.wait();

                  ELLE_ASSERT(!this->_avatar_to_fetch.empty());
                  auto user_id = *this->_avatar_to_fetch.begin();
                  auto id = this->_user_indexes.at(this->user(user_id).id);
                  try
                  {
                    this->_avatars.insert(
                      std::make_pair(id, this->_meta.icon(user_id)));
                    this->_avatar_to_fetch.erase(user_id);
                    this->enqueue(AvatarAvailableNotification(id));
                  }
                  catch (elle::Exception const& e)
                  {
                    ELLE_ERR("%s: unable to fetch %s avatar: %s", *this,
                             user_id, e.what());
                    // The UI will ask for the avatar again if it needs it, so
                    // remove the request from the queue if there's a problem.
                    this->_avatar_to_fetch.erase(user_id);
                  }

                  if (this->_avatar_to_fetch.empty())
                    this->_avatar_fetching_barrier.close();
                }
              }});

          // Users need to be resynchronized (cached) before we initialize the
          // transactions to ensure that users aren't fetched one by one.
          this->_user_resync(this->_synchronize_response->swaggers, true);
          ELLE_TRACE("initialize transaction")
            this->_transactions_init();
          ELLE_TRACE("connection")
            this->on_connection_changed(
              ConnectionState{true, elle::Error(""), false},
              true);
          this->_polling_thread.reset(
            new reactor::Thread{
              scheduler,
                "poll",
                [&]
                {
                  this->_logged_in.wait();
                  while (true)
                  {
                    try
                    {
                      this->handle_notification(this->_trophonius->poll());
                    }
                    catch (elle::Exception const& e)
                    {
                      ELLE_ERR("%s: an error occured in trophonius, login is " \
                               "required: %s", *this, elle::exception_string());
                      this->_kick_out(false, e.what());
                      return;
                    }

                    ELLE_TRACE("%s: notification pulled", *this);
                  }
                }});

          finally_logout.abort();
        };
        ELLE_TRACE("%s: logged in", *this);
        this->_logged_in.open();
        return true;
      }
      #define RETHROW(ExceptionType)                               \
      catch(ExceptionType const& e)                                \
      { /* Permanent failure, abort*/                              \
        this->enqueue(ConnectionStatus(false, false, e.what()));   \
        ELLE_WARN("%s: login fatal failure: %s", *this, e);        \
        this->_logged_out.open();                                  \
        if (_login_watcher_thread)                                 \
          _login_watcher_thread->raise(std::make_exception_ptr(e));\
        return false;                                                    \
      }
      RETHROW(infinit::state::CredentialError)
      RETHROW(infinit::state::UnconfirmedEmailError)
      RETHROW(infinit::state::VersionRejected)
      RETHROW(infinit::state::AlreadyLoggedIn)
      RETHROW(infinit::state::MissingEmail)
      RETHROW(infinit::state::EmailAlreadyRegistered)
      #undef RETHROW
      catch(elle::Exception const& e)
      { // Assume temporary failure and retry
        this->enqueue(ConnectionStatus(false, true, e.what()));
        ELLE_WARN("%s: login failure: %s", *this, e);
        // XX notify
        boost::random::mt19937 rng;
        rng.seed(static_cast<unsigned int>(std::time(0)));
        boost::random::uniform_int_distribution<> random(100, 150);
        auto const delay =
          reconnection_cooldown() * random(rng) / 100;
        ELLE_LOG("%s: will retry reconnection in %s", *this, delay);
        reactor::sleep(delay);
      }
    }

    void
    State::disconnect()
    {
      ELLE_TRACE_SCOPE("%s: disconnect", *this);
      if (this->_trophonius)
        this->_trophonius->disconnect();
    }

    void
    State::logout()
    {
      if (this->_login_thread)
        this->_login_thread->terminate_now();
      reactor::Lock l(this->_login_mutex);
      this->_logged_in.close();
      ELLE_TRACE_SCOPE("%s: logout", *this);

      this->_cleanup();
      // After cleanup, all containers are empty.
      // Do not use lazy accessor during the logout phase.

      ELLE_DEBUG("%s: cleaned up", *this);

      if (!this->logged_in_to_meta())
      {
        ELLE_DEBUG("%s: state was not logged in", *this);
        this->_logged_out.open();
        return;
      }

      try
      {
        reactor::Barrier timed_out;
        reactor::Duration timeout = 10_sec;
        bool logout_success =
          elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
          {
            bool success = false;
            scope.run_background("logout with timeout", [&]
            {
              this->_meta.logout();
              success = true;
              ELLE_TRACE("%s: logged out", *this);
              this->_metrics_reporter->user_logout(true, "");
            });
            scope.wait(timeout);
            return success;
          };
        if (!logout_success)
        {
          ELLE_WARN("logout failed, timed out after %s", timeout);
          this->_meta.logged_in(false);
          this->_metrics_reporter->user_logout(false, "timed out");
        }
      }
      catch (elle::Exception const&)
      {
        ELLE_WARN("logout failed, ignore exception: %s",
                  elle::exception_string());
        this->_meta.logged_in(false);
        this->_metrics_reporter->user_logout(false, elle::exception_string());
      }

      if (this->_trophonius)
        this->_trophonius->disconnect();
      this->_logged_out.open();
    }

    void
    State::clean()
    {
      if (this->_logged_in)
        this->logout();
      else
        this->_cleanup();
    }

    void
    State::_cleanup()
    {
      ELLE_TRACE_SCOPE("%s: cleaning up the state", *this);
      if (this->_metrics_heartbeat_thread != nullptr)
      {
        this->_metrics_heartbeat_thread->terminate_now();
        this->_metrics_heartbeat_thread.reset();
      }
      elle::SafeFinally clean_containers(
        [&]
        {
          ELLE_DEBUG("disconnect trophonius")
            if (this->_trophonius)
              this->_trophonius->disconnect();

          ELLE_DEBUG("remove pending callbacks")
            while (!this->_runners.empty())
              this->_runners.pop();

          ELLE_DEBUG("clear transactions")
            this->_transactions_clear();

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

          ELLE_DEBUG("clear device (%s)", this->_device.get())
            this->_device.reset();

          ELLE_DEBUG("clear me (%s)", this->_me.get())
            this->_me.reset();

          ELLE_DEBUG("clear passport (%s)", this->_passport.get())
            this->_passport.reset();

          ELLE_DEBUG("clear identity (%s)", this->_identity.get())
            this->_identity.reset();
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

    void
    State::register_(std::string const& fullname,
                     std::string const& email,
                     std::string const& password,
                     boost::optional<std::string> device_push_token,
                     boost::optional<std::string> country_code,
                     boost::optional<std::string> device_model,
                     boost::optional<std::string> device_name,
                     boost::optional<std::string> device_language)
    {
      // !WARNING! Do not log the password.
      ELLE_TRACE_SCOPE("%s: register as %s: email %s",
                       *this, fullname, email);

      std::string lower_email = email;
      std::transform(lower_email.begin(),
                     lower_email.end(),
                     lower_email.begin(),
                     ::tolower);

      // Logout first, and ignore errors.
      try { this->logout(); } catch (std::exception const&) {}

      std::string error_details = "";

      elle::SafeFinally register_failed{[this, lower_email, &error_details] {
          infinit::metrics::Reporter::metric_sender_id(lower_email);
          this->_metrics_reporter->user_register(false, error_details);
        }};

      try
      {
        ELLE_DEBUG("register");
        boost::optional<std::string> referral_code_opt;
        if (this->_referral_code.length())
          referral_code_opt = this->_referral_code;
        auto res =
          this->meta(false).register_(
            lower_email,
            fullname,
            password,
            referral_code_opt);
        register_failed.abort();
        ELLE_DEBUG("registered new user %s <%s>", fullname, lower_email);
        infinit::metrics::Reporter::metric_sender_id(res.id);
        this->metrics_reporter()->user_register(
          true, "", "", res.ghost_code, res.referral_code);
        this->_referral_code = "";
      }
      catch (elle::Error const& error)
      {
        error_details = error.what();
        throw;
      }
      this->login(lower_email, password, device_push_token, country_code,
                  device_model, device_name, device_language);

    }

    void
    State::facebook_connect(
      std::string const& facebook_token,
      std::unique_ptr<infinit::oracles::trophonius::Client> trophonius,
      boost::optional<std::string> preferred_email,
      boost::optional<std::string> device_push_token,
      boost::optional<std::string> country_code,
      boost::optional<std::string> device_model,
      boost::optional<std::string> device_name,
      boost::optional<std::string> device_language,
      reactor::DurationOpt timeout)
    {
      this->_login_with_timeout(
      [&] {
        auto tropho = elle::utility::move_on_copy(std::move(trophonius));
        boost::optional<std::string> referral_code_opt;
        if (this->_referral_code.length())
          referral_code_opt = this->_referral_code;
        this->_login([&] {
          auto res = this->_meta.facebook_connect(facebook_token,
                                                  this->device_uuid(),
                                                  preferred_email,
                                                  device_push_token,
                                                  country_code,
                                                  device_model,
                                                  device_name,
                                                  device_language);
          if (res.account_registered && this->_metrics_reporter)
          {
            this->_metrics_reporter->user_register(
              true, "", "facebook", res.ghost_code, res.referral_code);
          }
          return res;
          },
          tropho,
          // Password.
          [&] {
            return "";
          },
          [&] (bool success, std::string const& failure_reason) {
            this->_metrics_reporter->facebook_connect(success, failure_reason);
            this->_referral_code = "";
          });
      },
      timeout);
       this->_metrics_reporter->facebook_connect(true, "");
    }

    void
    State::facebook_connect(std::string const& token,
                            boost::optional<std::string> preferred_email,
                            boost::optional<std::string> device_push_token,
                            boost::optional<std::string> country_code,
                            boost::optional<std::string> device_model,
                            boost::optional<std::string> device_name,
                            boost::optional<std::string> device_language)
    {
      return this->facebook_connect(
        token, TrophoniusClientPtr{},
        preferred_email, device_push_token, country_code,
        device_model, device_name, device_language);
    }

    void
    State::add_facebook_account(std::string const& facebook_token)
    {
      this->meta().add_facebook_account(facebook_token);
    }

    Self const&
    State::me() const
    {
      ELLE_ASSERT_NEQ(this->_me, nullptr);
      return *this->_me;
    }

    void
    State::update_me()
    {
      this->_me.reset(new Self(this->meta().self()));
      this->me();
    }

    void
    State::set_avatar(boost::filesystem::path const& image_path)
    {
      if (!boost::filesystem::exists(image_path))
        throw Exception(gap_error,
                        elle::sprintf("file not found at %s", image_path));

      std::ifstream stream{image_path.string()};
      std::istreambuf_iterator<char> eos;
      std::istreambuf_iterator<char> iit(stream);   // stdin iterator

      elle::Buffer output;
      std::copy(iit, eos, output.begin());

      this->set_avatar(output);
    }

    void
    State::set_avatar(elle::Buffer const& avatar)
    {
      this->meta().icon(avatar);
    }

    void
    State::set_output_dir(std::string const& dir, bool fallback)
    {
      if (!fs::exists(dir))
        throw Exception{gap_error, "directory doesn't exist."};

      if (!fs::is_directory(dir))
        throw Exception{gap_error, "not a directory."};

      this->_output_dir = dir;

      this->_metrics_reporter->user_changed_download_dir(fallback);
    }

    void
    State::synchronize()
    {
      this->on_connection_changed(
      ConnectionState{true, elle::Error(""), false}, false);
    }

    void
    State::on_connection_changed(ConnectionState const& connection_state,
                                 bool first_connection)
    {
      ELLE_TRACE_SCOPE(
        "%s: %sconnection %s %s %s",
        *this,
        first_connection? "first " : "re",
        connection_state.connected ? "established" : "lost",
        connection_state.still_trying ? "for now" : "forever",
        connection_state.connected ? "" : connection_state.last_error.what());

      if (connection_state.connected)
      {
        bool resynched{false};
        do
        {
          if (!this->logged_in_to_meta())
          {
            ELLE_TRACE("%s: not logged in, aborting", *this);
            return;
          }
          // Link with tropho might have changed.
          if (!first_connection)
          {
            ELLE_TRACE("reset transactions")
              for (auto& t: this->_transactions)
              {
                if (!t.second->final())
                  t.second->reset();
                else
                  ELLE_DEBUG("ignore finalized transaction %s", t.second);
              }
           this->_synchronize_response.reset(
             new infinit::oracles::meta::SynchronizeResponse{this->meta().synchronize(false)});
           this->_account(this->_synchronize_response->account);
           this->_devices(this->_synchronize_response->devices);
           this->_external_accounts(
             this->_synchronize_response->external_accounts);
          }
          // This is never the first call to _user_resync as the function is
          // called in login.
          this->_user_resync(this->_synchronize_response->swaggers, false);
          this->_peer_transaction_resync(
            this->_synchronize_response->transactions, first_connection);
          this->_link_transaction_resync(
            this->_synchronize_response->links, first_connection);

          resynched = true;
          ELLE_TRACE("Opening logged_in barrier");
          this->_logged_in.open();
          this->_synchronized.signal();
        }
        while (!resynched);
        this->_ghost_code_use();
      }
      else
      { // not connected
        this->_logged_in.close();
        if (!connection_state.still_trying)
          logout();
      }
      this->enqueue<ConnectionStatus>(ConnectionStatus(
        connection_state.connected,
        connection_state.still_trying,
        connection_state.last_error.what()));
    }

    void
    State::on_reconnection_failed()
    {
      ELLE_TRACE_SCOPE("%s: reconnection failed, refetching trophonius", *this);
      try
      {
        auto tropho = this->_meta.trophonius();
        std::string host = tropho.host;
        int port = tropho.port_ssl;
        if (!this->_forced_trophonius_host.empty())
          host = this->_forced_trophonius_host;
        if (this->_forced_trophonius_port != 0)
          port = this->_forced_trophonius_port;
        this->_trophonius->server(host, port);
      }
      catch (infinit::state::CredentialError const&)
      {
        this->_on_invalid_trophonius_credentials();
      }
    }

    void
    State::_on_invalid_trophonius_credentials()
    {
      ELLE_WARN("%s: invalid trophonius credentials", *this);
      this->_kick_out(false, "Invalid trophonius credentials");
    }

    void
    State::handle_notification(
      std::unique_ptr<infinit::oracles::trophonius::Notification>&& notif)
    {
      ELLE_TRACE_SCOPE("%s: new notification %s", *this, *notif);
      // FIXME: ever heard of virtual methods ?
      switch (notif->notification_type)
      {
        case infinit::oracles::trophonius::NotificationType::configuration:
        {
          auto configuration =
            dynamic_cast<infinit::oracles::trophonius::ConfigurationNotification const*>(notif.get());
          ELLE_ASSERT(configuration);
          this->_apply_configuration(std::move(configuration->configuration));
          break;
        }
        case infinit::oracles::trophonius::NotificationType::user_status:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::UserStatusNotification const*>(
              notif.get()) != nullptr);
          this->_on_swagger_status_update(
            *static_cast<infinit::oracles::trophonius::UserStatusNotification const*>(
              notif.get()));
          break;
        case infinit::oracles::trophonius::NotificationType::link_transaction:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::LinkTransactionNotification const*>(notif.get()) != nullptr);
          this->_on_transaction_update(
            std::make_shared<infinit::oracles::LinkTransaction>(static_cast<infinit::oracles::trophonius::LinkTransactionNotification&>(*notif)));
          break;
        case infinit::oracles::trophonius::NotificationType::peer_transaction:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::PeerTransactionNotification const*>(notif.get()) != nullptr);
          this->_on_transaction_update(
            std::make_shared<infinit::oracles::PeerTransaction>(static_cast<infinit::oracles::trophonius::PeerTransactionNotification&>(*notif)));
          break;
        case infinit::oracles::trophonius::NotificationType::new_swagger:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::NewSwaggerNotification const*>(
              notif.get()) != nullptr);
          this->_on_new_swagger(
            *static_cast<infinit::oracles::trophonius::NewSwaggerNotification const*>(
              notif.get()));
          break;
        case infinit::oracles::trophonius::NotificationType::deleted_swagger:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::DeletedSwaggerNotification const*>(
              notif.get()) != nullptr);
          this->_on_deleted_swagger(
            *static_cast<infinit::oracles::trophonius::DeletedSwaggerNotification const*>(
              notif.get()));
        break;
        case infinit::oracles::trophonius::NotificationType::deleted_favorite:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::DeletedFavoriteNotification const*>(
              notif.get()) != nullptr);
          this->_on_deleted_favorite(
            *static_cast<infinit::oracles::trophonius::DeletedFavoriteNotification const*>(
              notif.get()));
        break;
        case infinit::oracles::trophonius::NotificationType::peer_connection_update:
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::PeerReachabilityNotification const*>(
              notif.get()) != nullptr);
          this->_on_peer_reachability_updated(
            *static_cast<infinit::oracles::trophonius::PeerReachabilityNotification const*>(
              notif.get()));
          break;
        case infinit::oracles::trophonius::NotificationType::invalid_credentials:
          this->_on_invalid_trophonius_credentials();
          break;
        case infinit::oracles::trophonius::NotificationType::model_update:
        {
          auto n =
            elle::cast<infinit::oracles::trophonius::ModelUpdateNotification>::
            runtime(notif);
          ELLE_ASSERT(n.get());
          elle::serialization::json::SerializerIn input(n->json, false);
          DasModel::Update u(input);
          ELLE_DEBUG("%s: apply model update", *this);
          ELLE_DUMP("new model: %s", u);
          u.apply(this->_model);
          // FIXME: Remove manual call to change when das handles parent object
          // changed signal.
          this->_model.account.changed().operator()(this->_model.account);
          break;
        }
        case infinit::oracles::trophonius::NotificationType::paused:
        {
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::PausedNotification const*>(
              notif.get()) != nullptr);
          this->_on_transaction_paused(
            *static_cast<infinit::oracles::trophonius::PausedNotification const*>(
              notif.get()));
          break;
        }
        case infinit::oracles::trophonius::NotificationType::message:
        {
          ELLE_ASSERT(
            dynamic_cast<infinit::oracles::trophonius::MessageNotification const*>(
              notif.get()) != nullptr);
          auto& message =
            *static_cast<infinit::oracles::trophonius::MessageNotification const*>(
              notif.get());
          this->_message_received(message.message);
          break;
        }
        case infinit::oracles::trophonius::NotificationType::none:
        case infinit::oracles::trophonius::NotificationType::network_update:
        case infinit::oracles::trophonius::NotificationType::ping:
        case infinit::oracles::trophonius::NotificationType::connection_enabled:
        case infinit::oracles::trophonius::NotificationType::suicide:
          ELLE_ERR("%s: unhandled notification of type %s received",
                   *this, notif->notification_type);
          elle::unreachable();
      }
    };

    /*-------------------.
    | External Callbacks |
    `-------------------*/

    void
    State::poll() const
    {
      ELLE_DEBUG_SCOPE("poll %s notification(s)", this->_runners.size());
      while (true)
      {
        std::unique_ptr<_Runner> runner;
        {
          std::lock_guard<std::mutex> lock{this->_poll_lock};
          if (this->_runners.empty())
            return;
          std::swap(runner, this->_runners.front());
          this->_runners.pop();
        }
        (*runner)();
      }
    }

    /*--------.
    | Account |
    `--------*/
    Account
    State::account() const
    {
      return this->_model.account;
    }

    void
    State::_account(Account const& account)
    {
      ELLE_TRACE_SCOPE("reset %s with %s", this->_model, account);
      this->_model.account = account;
      if (this->metrics_reporter())
      {
        std::string new_plan(elle::sprintf("%s", this->_model.account.plan));
        if (infinit::metrics::Reporter::metric_sender_plan() != new_plan)
          infinit::metrics::Reporter::metric_sender_plan(new_plan);
      }
    }

    /*------------------.
    | External Accounts |
    `------------------*/
    std::vector<ExternalAccount const*>
    State::external_accounts() const
    {
      std::vector<ExternalAccount const*> res;
      res.reserve(this->_model.external_accounts.size());
      for (auto const& account: this->_model.external_accounts)
        res.push_back(&account);
      return res;
    }

    void
    State::_external_accounts(std::vector<ExternalAccount> const& accounts)
    {
      ELLE_TRACE_SCOPE("reset %s with %s", this->_model, accounts);
      this->_model.external_accounts.reset(accounts);
      ELLE_ASSERT_EQ(this->_model.external_accounts.size(), accounts.size());
    }

    /*--------.
    | Devices |
    `--------*/

    std::vector<Device const*>
    State::devices() const
    {
      std::vector<Device const*> res;
      res.reserve(this->_model.devices.size());
      for (auto const& device: this->_model.devices)
        res.push_back(&device);
      return res;
    }

    void
    State::_on_device_added(Device const& device) const
    {}

    void
    State::_on_device_removed(elle::UUID id)
    {
      ELLE_TRACE_SCOPE("device %s removed from list %s",
        id, this->_model.devices);
      if (id == this->_device_uuid)
        this->_kick_out(false, "Your device has been deleted");
    }

    void
    State::_on_devices_reseted()
    {
      ELLE_TRACE_SCOPE("devices reseted: %s", this->_model);
      if (this->_model.devices.empty() ||
        std::find_if(this->_model.devices.begin(),
                     this->_model.devices.end(),
                     [&] (Device const& device)
                     {
                        return device.id == this->_device->id;
                     }) == this->_model.devices.end())
        this->_on_device_removed(this->_device->id);
    }

    void
    State::_devices(std::vector<Device> const& devices)
    {
      ELLE_TRACE_SCOPE("reset %s with %s", this->_model, devices);
      this->_model.devices.reset(devices);
      ELLE_ASSERT_EQ(this->_model.devices.size(), devices.size());
      auto it = std::find_if(
        this->_model.devices.begin(),
        this->_model.devices.end(),
        [&] (Device const& device)
        {
          return device.id == this->_device->id;
        });
      if (it != this->_model.devices.end())
      {
        it->second.name.changed().connect([&] (std::string const& name)
        {
          this->_device->name = name;
        });
        // Activate the changed method to update the local device.
        it->second.name.changed()(it->second.name);
      }
    }

    /*--------------.
    | Configuration |
    `--------------*/

    void
    State::Configuration::serialize(elle::serialization::Serializer& s)
    {
      s.serialize("s3", this->s3);
      s.serialize("enable_file_mirroring", this->enable_file_mirroring);
      s.serialize("max_mirror_size", this->max_mirror_size);
      s.serialize("max_compress_size", this->max_compress_size);
      s.serialize("max_cloud_buffer_size", this->max_cloud_buffer_size);
      s.serialize("disable_upnp", this->disable_upnp);
      s.serialize("features", this->features);
    }

    void
    State::Configuration::S3::serialize(elle::serialization::Serializer& s)
    {
      s.serialize("multipart_upload", this->multipart_upload);
    }

    void
    State::Configuration::S3::MultipartUpload::serialize(
      elle::serialization::Serializer& s)
    {
      s.serialize("chunk_size", this->chunk_size);
      s.serialize("parallelism", this->parallelism);
    }

    void
    State::_apply_configuration(elle::json::Object json)
    {
      ELLE_TRACE_SCOPE("%s: apply configuration: %s", *this, json);
      if (json.find("features") != json.end())
        this->_configuration.features.clear();
      elle::serialization::json::SerializerIn input(json);
      input.partial(true);
      this->_configuration.serialize(input);
      infinit::metrics::Reporter::metric_features(
        this->_configuration.features);
      std::ofstream fconfig(this->local_configuration().configuration_path());
      elle::json::write(fconfig, json);
    }

    void
    State::change_password(std::string const& password,
                           std::string const& new_password)
    {
      this->meta().change_password(password, new_password);
    }

    /*------------.
    | Invitations |
    `------------*/
    surface::gap::PlainInvitation
    State::plain_invite_contact(std::string const& identifier)
    {
      auto const& meta_invite = this->meta().plain_invite_contact(identifier);
      return surface::gap::PlainInvitation(meta_invite.identifier(),
                                           meta_invite.ghost_code(),
                                           meta_invite.ghost_profile_url());
    }

    void
    State::send_invite(std::string const& destination,
                       std::string const& message,
                       std::string const& ghost_code)
    {
      this->meta().send_invite(destination, message, ghost_code);
    }

    /*-----------.
    | Ghost code |
    `-----------*/

    void
    State::ghost_code_use(std::string const& code, bool was_link)
    {
      ELLE_DEBUG_SCOPE("%s: register ghost codes: %s", *this, code);
      this->_ghost_codes.push_back(GhostCode{code, was_link});
      this->_ghost_code_snapshot();
      if (this->_logged_in)
        this->_ghost_code_use();
    }

    void
    State::_ghost_code_snapshot()
    {
      ELLE_DEBUG_SCOPE("%s: snapshot ghost codes", *this);
      boost::filesystem::path dir(
        this->local_configuration().non_persistent_config_dir());
      elle::AtomicFile snapshot(dir / "ghost-code.snapshot");
      elle::With<elle::AtomicFile::Write>(snapshot.write())
        << [&] (elle::AtomicFile::Write& write)
      {
        elle::serialization::json::SerializerOut output(write.stream());
        output.serialize("codes", this->_ghost_codes);
      };
    }

    void
    State::_ghost_code_use()
    {
      ELLE_TRACE_SCOPE("%s: consume ghost codes", *this);
      while (!this->_ghost_codes.empty())
      {
        auto code = this->_ghost_codes.back();
        ELLE_DEBUG_SCOPE("%s: consume %s", *this, code.code);
        try
        {
          this->meta().use_ghost_code(code.code);
          this->_ghost_code_used(code.code, true, "");
          this->metrics_reporter()->user_used_ghost_code(
            true, code.code, code.was_link, "");
        }
        catch (infinit::state::GhostCodeAlreadyUsed const&)
        {
          ELLE_DEBUG("%s: code was already used", *this);
          this->_ghost_code_used(code.code, false, "already used");
          this->metrics_reporter()->user_used_ghost_code(
            false, code.code, code.was_link, "already used");
        }
        catch (elle::Error const& e)
        {
          // FIXME: what about it ? just ignore it ?
          ELLE_WARN("%s: unable to use code %s: %s", *this, code.code, e);
          auto reason = elle::sprintf("%s", e);
          this->_ghost_code_used(code.code, false, reason);
          this->metrics_reporter()->user_used_ghost_code(
            false, code.code, code.was_link, reason);
        }
        this->_ghost_codes.pop_back();
        this->_ghost_code_snapshot();
      }
    }

    /*------------.
    | Fingerprint |
    `------------*/

    void
    State::fingerprint_add(std::string const& fingerprint)
    {
      ELLE_TRACE_SCOPE("%s: handle fingerprint: %s", *this, fingerprint);
      if (fingerprint.substr(0, 7) == "INVITE:")
      {
        auto code = fingerprint.substr(7, fingerprint.find_first_of(' ') - 7);
        this->ghost_code_use(code, true);
      }
      else if (fingerprint != "0123456789ABCDEF")
      {
        this->_referral_code = fingerprint;
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
    operator <<(std::ostream& out, NotificationType const& t)
    {
      switch (t)
      {
        case NotificationType_PeerTransactionUpdate:
          return out << "Peer Transaction Update";
        case NotificationType_UserStatusUpdate:
          return out << "User Status Update";
        case NotificationType_NewSwagger:
          return out << "New Swagger";
        case NotificationType_DeletedSwagger:
          return out << "Deleted Swagger";
        case NotificationType_DeletedFavorite:
          return out << "Deleted Favorite";
        case NotificationType_ConnectionStatus:
          return out << "ConnectionStatus";
        case NotificationType_AvatarAvailable:
          return out << "Avatar Available";
        case NotificationType_TrophoniusUnavailable:
          return out << "Trophonius Unavailable";
        case NotificationType_LinkTransactionUpdate:
          return out << "Link Update";
        case NotificationType_TransactionRecipientChanged:
          return out << "Transaction Recipient Changed";
      }

      return out;
    }

    std::ostream&
    operator <<(std::ostream& out,
                State::ConnectionStatus const& s)
    {
      return out << elle::sprintf("ConnectionStatus(%s, %s, %s)",
        s.status, s.still_trying, s.last_error);
    }
  }
}

std::ostream&
operator <<(std::ostream& out,
            gap_TransactionStatus status)
{
  switch (status)
  {
    case gap_transaction_new:
      out << "new";
      break;
    case gap_transaction_on_other_device:
      out << "on_other_device";
      break;
    case gap_transaction_waiting_accept:
      out << "waiting_accept";
      break;
    case gap_transaction_waiting_data:
      out << "waiting_data";
      break;
    case gap_transaction_connecting:
      out << "connecting";
      break;
    case gap_transaction_transferring:
      out << "transferring";
      break;
    case gap_transaction_cloud_buffered:
      out << "cloud_buffered";
      break;
    case gap_transaction_finished:
      out << "finished";
      break;
    case gap_transaction_failed:
      out << "failed";
      break;
    case gap_transaction_canceled:
      out << "canceled";
      break;
    case gap_transaction_rejected:
      out << "rejected ";
      break;
    case gap_transaction_deleted:
      out << "deleted ";
      break;
    case gap_transaction_paused:
      out << "paused";
      break;
    case gap_transaction_payment_required:
      out << "payment required";
      break;
  }
  return out;
}
