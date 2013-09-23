#include <CrashReporter.hh>

#include <fstream>
#include <map>
#include <signal.h>

#include <boost/asio/io_service.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <curly/curly.hh>
#include <elle/format/json.hh>
#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/system/platform.hh>
#include <reactor/scheduler.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("elle.CrashReporter");

namespace elle
{
  namespace signal
  {
    namespace
    {
      // It seems that strsignal from signal.h isn't portable on every os.
      // This map contains the main posix signals and make can easy be multi-
      // platform by using defines for specific os.
      std::string const&
      strsignal(int signal)
      {
        static const std::unordered_map<int, std::string> bind{
          {
            {SIGHUP,  "SIGHUP"},  // Hangup detected on controlling terminal or death of controlling process
            {SIGINT,  "SIGINT"},  // Interrupt from keyboard
            {SIGQUIT, "SIGQUIT"}, // Quit from keyboard
            {SIGILL,  "SIGILL"},  // Illegal Instruction
            {SIGABRT, "SIGABRT"}, // Abort signal from abort(3)
            {SIGFPE,  "SIGFPE"},  // Floating point exception
            {SIGKILL, "SIGKILL"}, // Kill signal
            {SIGSEGV, "SIGSEGV"}, // Invalid memory reference
            {SIGPIPE, "SIGPIPE"}, // Broken pipe: write to pipe with no readers
            {SIGALRM, "SIGALRM"}, // Timer signal from alarm(2)
            {SIGTERM, "SIGTERM"}, // Termination signal
            {SIGCHLD, "SIGCHLD"}, // Child stopped or terminated
            {SIGCONT, "SIGCONT"}, // Continue if stopped
            {SIGSTOP, "SIGSTOP"}, // Stop process
            {SIGTSTP, "SIGTSTP"}, // Stop typed at tty
            {SIGTTIN, "SIGTTIN"}, // tty input for background process
            {SIGTTOU, "SIGTTOU"}, // tty output for background process
          }
        };

        return bind.at(signal);
      }
    } // End of anonymous namespace.

    class ScopedGuard::Impl
    {
      /*---------------.
      | Initialization |
      `---------------*/
    public:
      virtual ~Impl()
      {}

      virtual
      void
      launch(std::vector<int> const& sigs) = 0;

      /*------------.
      | Destruction |
      `------------*/
      virtual
      void
      release() = 0;
    };

    //- Async impl -------------------------------------------------------------
    static
    void
    _wrap(boost::system::error_code const& error,
          int sig,
          ScopedGuard::Handler const& handler)
    {
      if (!error)
      {
        ELLE_WARN("signal caught: %s.", elle::signal::strsignal(sig));
        handler(sig);
      }
      else if (error != boost::system::errc::operation_canceled)
      {
        ELLE_WARN("Error: %d - Sig: %d", error, sig);
      }
    }

    // XXX: Should be templated on Handler maybe.
    class AsyncImpl:
      public ScopedGuard::Impl
    {
      typedef ScopedGuard::Handler Handler;

      Handler _handler;
      boost::asio::signal_set _signals;

    public:
      AsyncImpl(reactor::Scheduler& sched,
                Handler const& handler):
        _handler{handler},
        _signals{sched.io_service()}
      {}

    public:
      virtual
      void
      launch(std::vector<int> const& sigs) override
      {
        ELLE_TRACE("launching guard");
        for (int sig: sigs)
        {
          ELLE_DEBUG("handling %s (%s) asynchronously.", strsignal(sig), sig);
          this->_signals.add(sig);
        }

        ELLE_DEBUG("now waiting for signals...");
        this->_signals.async_wait(
          [&](boost::system::error_code const& error,
              int sig)
          {
            if (error != boost::system::errc::operation_canceled)
            {
              this->release();
              _wrap(error, sig, this->_handler);
            }
          });
      }

      virtual
      void
      release() override
      {
        ELLE_TRACE("releasing guard");
        //XXX We should check errors.
        this->_signals.cancel();
      }
    };

    class SyncImpl:
      public ScopedGuard::Impl
    {
      typedef sighandler_t Handler;

      Handler _handler;
      std::vector<int> _sigs;
    public:
      SyncImpl(Handler const& handler):
        _handler{handler}
      {}

    public:
      virtual
      void
      launch(std::vector<int> const& sigs) override
      {
        this->_sigs = sigs;

        for (int sig: this->_sigs)
        {
          ELLE_DEBUG("handling %s synchronously", strsignal(sig));
          // To avoid handler to be override.
          ELLE_ASSERT(::signal(sig, this->_handler) == SIG_DFL);
        }
      }

      virtual
      void
      release() override
      {
        for (int sig: this->_sigs)
        {
          ELLE_DEBUG("releasing %s (sync).", strsignal(sig));
          // Restore handler, according to the hypothesis that nobody erased it.
          ELLE_ASSERT(::signal(sig, SIG_DFL) == this->_handler);
        }
      }
    };

    ScopedGuard::ScopedGuard(reactor::Scheduler& sched,
                             std::vector<int> const& sigs,
                             Handler const& handler):
      _impl{new AsyncImpl{sched, handler}}
    {
      this->_impl->launch(sigs);
    }

    ScopedGuard::ScopedGuard(std::vector<int> const& sigs,
                             sighandler_t const& handler):
      _impl{new SyncImpl{handler}}
    {
      this->_impl->launch(sigs);
    }

    ScopedGuard::~ScopedGuard()
    {
      this->_impl->release();
    }
  } // End of signal.

  namespace crash
  {

    Handler::Handler(std::string const& host,
                     int port,
                     std::string const& name,
                     bool quit):
                     _host(host),
                     _port(port),
                     _name{name},
                     _quit{quit}
    {}

    Handler::Handler(std::string const& host,
                     int port,
                     std::string const& name,
                     bool quit,
                     int argc,
                     char** argv):
      Handler(host, port, name, quit)
    {
      for (int i = 1; i < argc; ++i)
      {
        if (argv[i] == NULL)
          break;

        this->_name += " ";
        this->_name += argv[i];
      }
    }

    Handler::~Handler()
    {}

    void
    Handler::operator() (int sig)
    {
      elle::crash::report
        (this->_host, this->_port, this->_name, elle::signal::strsignal(sig));

      if (this->_quit)
        ::exit(sig);
    }

    bool
    existing_report(std::string const& host,
                    uint16_t port,
                    std::string const& user_name,
                    std::string const& os_description,
                    std::string const& info,
                    std::string const& file)
    {
      ELLE_TRACE("Report old crash");

      elle::format::json::Array env_arr{};

      for (auto const& pair: elle::os::environ())
        if (boost::starts_with(pair.first, "ELLE_") or
            boost::starts_with(pair.first, "INFINIT_"))
          env_arr.push_back(pair.first + " = " + pair.second);

      elle::format::json::Dictionary request;

      request["user_name"] = user_name;
      request["client_os"] = os_description;
      request["more"] = info;
      request["env"] = env_arr;
      request["version"] = INFINIT_VERSION;
      request["email"] = elle::os::getenv("INFINIT_CRASH_DEST", "");
      request["file"] = file;
#ifdef INFINIT_PRODUCTION_BUILD
      request["send"] = true;
#else
      request["send"] = !request["email"].as<std::string>().empty();
#endif

      std::stringstream json_stream;
      request.repr(json_stream);
      std::string url = elle::sprintf("http://%s:%d/debug/existing-report",
                                      host,
                                      port);
      try
        {
          // We have to tricks because web.py doesn't handle chunked POST.
          auto post_config = curly::make_post();
          post_config.url(url);
          post_config.option(CURLOPT_POSTFIELDSIZE, json_stream.str().size());
          post_config.input(json_stream);
          post_config.user_agent("InfinitDesktop");
          post_config.headers({
            {"Content-type", "application/json"},
            {"Expect", ""}
          });
          curly::request r(std::move(post_config));
          return true;
        }
      catch (...)
        {
          ELLE_WARN("Unable to put on %s: '%s'", url, json_stream.str());
          return false;
        }
    }

    bool
    report(std::string const& host,
           uint16_t port,
           std::string const& module,
           std::string const& signal,
           elle::Backtrace const& bt,
           std::string const& info,
           std::string const& file)
    {
      ELLE_TRACE("Report crash");

      elle::format::json::Array bt_arr{}, env_arr{};
      for (auto const& t: bt)
        bt_arr.push_back(static_cast<std::string>(t));

      for (auto const& pair: elle::os::environ())
        if (boost::starts_with(pair.first, "ELLE_") or
            boost::starts_with(pair.first, "INFINIT_"))
          env_arr.push_back(pair.first + " = " + pair.second);

      elle::format::json::Dictionary request;

      request["module"] = module;
      request["signal"] = signal;
      request["backtrace"] = bt_arr;
      request["env"] = env_arr;
      request["version"] = INFINIT_VERSION;

      request["email"] = elle::os::getenv("INFINIT_CRASH_DEST", "");
#ifdef INFINIT_PRODUCTION_BUILD
      request["send"] = true;
#else
      request["send"] = !request["email"].as<std::string>().empty();
#endif
      request["more"] = info;
      request["file"] = file;

      std::stringstream json_stream;
      request.repr(json_stream);
      std::string url = elle::sprintf("http://%s:%d/debug/report", host, port);
      try
        {
          // We have to tricks because web.py doesn't handle chunked POST.
          auto post_config = curly::make_post();
          post_config.url(url);
          post_config.option(CURLOPT_POSTFIELDSIZE, json_stream.str().size());
          post_config.input(json_stream);
          post_config.user_agent("InfinitDesktop");
          post_config.headers({
            {"Content-type", "application/json"},
            {"Expect", ""}
          });
          curly::request r(std::move(post_config));
          return true;
        }
      catch (...)
        {
          ELLE_WARN("Unable to put on %s: '%s'", url, json_stream.str());
          return false;
        }
    }
  } // End of crash.
} // End of elle.
