#ifndef ELLE_CRASHREPORTER_HH
# define ELLE_CRASHREPORTER_HH

# include <cstring>

# include <boost/system/error_code.hpp>
# include <boost/asio/signal_set.hpp>
# include <boost/signals.hpp>

# include <elle/Backtrace.hh>

# include <reactor/fwd.hh>

# ifdef __APPLE__
  typedef void(*sighandler_t)(int);
# endif

namespace elle
{
  namespace signal
  {

    class ScopedGuard:
      private boost::noncopyable
    {
      /*---------.
      | Typedefs |
      `---------*/
    public:
      /// Handler prototype.
      typedef
      std::function<void (int)> Handler;

      /// Attributes.
    private:

      Handler _handler;

      /*-------------.
      | Construction |
      `-------------*/
    public:
      /// Initialize with signal list and handler (async).
      ScopedGuard(reactor::Scheduler& sched,
                  std::vector<int> const& sigs,
                  Handler const& handler);
      /// Initialize with signal list and handler (sync).
      ScopedGuard(std::vector<int> const& sigs,
                  sighandler_t const& handler);
      /// Destroy current handlers.
      ~ScopedGuard();

      class Impl;
      std::unique_ptr<Impl> _impl;
    };
  } // End of signal.

  namespace crash
  {

    class Handler
    {
    public:
      Handler(std::string const& host,
              int port,
              std::string const& name,
              bool quit);

      Handler(std::string const& host,
              int port,
              std::string const& name,
              bool quit,
              int argc,
              char** argv);

      virtual ~Handler();

      virtual
      void
      operator() (int sig);

    private:
      std::string _host;
      uint16_t _port;
      std::string _name;
      bool _quit;
    };

    bool
    report(std::string const& host,
           uint16_t port,
           std::string const& module,
           std::string const& signal = "",
           elle::Backtrace const& b = elle::Backtrace::current(),
           std::string const& info = "",
           std::string const& file = "");
  }

}

#endif
