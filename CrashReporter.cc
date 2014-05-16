#include <fstream>
#include <signal.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include <elle/archive/archive.hh>
#include <elle/filesystem/TemporaryDirectory.hh>
#include <elle/json/json.hh>
#include <elle/format/base64.hh>
#include <elle/format/gzip.hh>
#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <elle/os/path.hh>
#include <elle/system/platform.hh>
#ifndef INFINIT_WINDOWS
# include <elle/system/Process.hh>
#endif

#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

#include <common/common.hh>

#include <CrashReporter.hh>
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
            {SIGINT,  "SIGINT"},  // Interrupt from keyboard
            {SIGILL,  "SIGILL"},  // Illegal Instruction
            {SIGABRT, "SIGABRT"}, // Abort signal from abort(3)
            {SIGFPE,  "SIGFPE"},  // Floating point exception
            {SIGSEGV, "SIGSEGV"}, // Invalid memory reference
            {SIGTERM, "SIGTERM"}, // Termination signal
#ifndef INFINIT_WINDOWS
            {SIGKILL, "SIGKILL"}, // Kill signal
            {SIGHUP,  "SIGHUP"},  // Hangup detected on controlling terminal or death of controlling process
            {SIGQUIT, "SIGQUIT"}, // Quit from keyboard
            {SIGILL,  "SIGILL"},  // Illegal Instruction
            {SIGPIPE, "SIGPIPE"}, // Broken pipe: write to pipe with no readers
            {SIGALRM, "SIGALRM"}, // Timer signal from alarm(2)
            {SIGCHLD, "SIGCHLD"}, // Child stopped or terminated
            {SIGCONT, "SIGCONT"}, // Continue if stopped
            {SIGSTOP, "SIGSTOP"}, // Stop process
            {SIGTSTP, "SIGTSTP"}, // Stop typed at tty
            {SIGTTIN, "SIGTTIN"}, // tty input for background process
            {SIGTTOU, "SIGTTOU"}, // tty output for background process
#endif
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
    void
    static
    _send_report(std::string const& url,
                 std::string const& user_name,
                 std::string const& os_description,
                 std::string const& message,
                 std::string file)
    {
      ELLE_TRACE_SCOPE("send report %s to %s", message, url);
      std::vector<boost::any> env_arr;

      ELLE_DEBUG("store environment")
        for (auto const& pair: elle::os::environ())
        {
          if (boost::starts_with(pair.first, "ELLE_") ||
              boost::starts_with(pair.first, "INFINIT_"))
          {
            std::string line =
              elle::sprintf("%s = %s", pair.first, pair.second);
            env_arr.push_back(line);
          }
        }
      // Create JSON.
      elle::json::Object json_dict;
      {
        json_dict["user_name"] = user_name;
        json_dict["client_os"] = os_description;
        if (!message.empty())
          json_dict["message"] = message;
        json_dict["env"] = env_arr;
        json_dict["version"] = std::string(INFINIT_VERSION);
        std::string crash_dest = elle::os::getenv("INFINIT_CRASH_DEST", "");
        if (!crash_dest.empty())
          json_dict["email"] = crash_dest;
        json_dict["file"] = file;
# ifdef INFINIT_PRODUCTION_BUILD
        json_dict["send"] = true;
# else
        json_dict["send"] = !crash_dest.empty();
# endif
      }
      reactor::http::Request::Configuration conf{
        reactor::DurationOpt(300_sec),
        reactor::DurationOpt(),
        reactor::http::Version(reactor::http::Version::v10)};
      conf.ssl_verify_host(false);
      reactor::Scheduler sched;
      reactor::Thread thread(
        sched, "upload report",
        [&]
        {
          try
          {
            reactor::http::Request request(
              url,
              reactor::http::Method::POST,
              "application/json",
              conf);
            elle::json::write(request, json_dict);
            request.wait();
            if (request.status() != reactor::http::StatusCode::OK)
            {
              ELLE_ERR("error while posting report to %s: (%s) %s",
                       url, request.status(), request.response().string());
            }
          }
          catch (...)
          {
            ELLE_ERR("unable to post report to %s", url);
          }
        });
      sched.run();
    }

    static
    std::string
    _to_base64(boost::filesystem::path const& source)
    {
      ELLE_TRACE_SCOPE("turn %s to base 64", source.string());
      std::stringstream base64;
      // Scope to flush Stream(s).
      {
        elle::format::base64::Stream encoder(base64);
        boost::filesystem::ifstream source_stream(source, std::ios::binary);
        std::copy(std::istreambuf_iterator<char>(source_stream),
                  std::istreambuf_iterator<char>(),
                  std::ostreambuf_iterator<char>(encoder));
      }
      return base64.str();
    }

    void
    existing_report(std::string const& meta_protocol,
                    std::string const& meta_host,
                    uint16_t meta_port,
                    std::vector<std::string> const& files,
                    std::string const& user_name,
                    std::string const& os_description,
                    std::string const& info)
    {
      ELLE_TRACE("report last crash");
      std::string url = elle::sprintf("%s://%s:%d/debug/report/backtrace",
                                      meta_protocol,
                                      meta_host,
                                      meta_port);
      elle::filesystem::TemporaryDirectory tmp;
      boost::filesystem::path destination(tmp.path() / "report.tar.bz2");
      std::vector<boost::filesystem::path> archived;
      for (auto path_str: files)
      {
        boost::filesystem::path path(path_str);
        if (boost::filesystem::exists(path))
          archived.push_back(path);
      }
      elle::archive::archive(elle::archive::Format::tar_gzip,
                             archived,
                             destination);
      _send_report(url, user_name, os_description, "",
                   _to_base64(destination));
    }

    void
    transfer_failed_report(std::string const& meta_protocol,
                           std::string const& meta_host,
                           uint16_t meta_port,
                           std::string const& user_name)
    {
      ELLE_TRACE("transaction failed report");
      std::string os_description{common::system::platform()};
      std::string url = elle::sprintf("%s://%s:%s/debug/report/transaction",
                                      meta_protocol,
                                      meta_host,
                                      meta_port);
      elle::filesystem::TemporaryDirectory tmp;
      boost::filesystem::path destination(tmp.path() / "report.tar.bz2");
      boost::filesystem::path infinit_home_path;
      elle::archive::archive(elle::archive::Format::tar_gzip,
                             {common::infinit::home()},
                             destination);
      _send_report(url, user_name, os_description, "",
                   _to_base64(destination));
    }

    void
    user_report(std::string const& meta_protocol,
                std::string const& meta_host,
                uint16_t meta_port,
                std::string const& user_name,
                std::string const& os_description,
                std::string const& message,
                std::string const& user_file)
    {
      ELLE_TRACE_SCOPE("user report");
      std::string url = elle::sprintf("%s://%s:%s/debug/report/user",
                                      meta_protocol,
                                      meta_host,
                                      meta_port);
      elle::filesystem::TemporaryDirectory tmp;
      boost::filesystem::path destination(tmp.path() / "report.tar.bz2");
      boost::filesystem::path infinit_home_path;
      if (user_file.size() == 0)
      {
        elle::archive::archive(elle::archive::Format::tar_gzip,
                               {common::infinit::home()},
                               destination);
      }
      else
      {
        elle::archive::archive(elle::archive::Format::tar_gzip,
                               {user_file, common::infinit::home()},
                               destination);
      }
      _send_report(url, user_name, os_description, message,
                   _to_base64(destination));
    }
  }
}
