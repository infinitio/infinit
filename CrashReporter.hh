#ifndef ELLE_CRASHREPORTER_HH
# define ELLE_CRASHREPORTER_HH

# include <cstring>

# include <elle/Backtrace.hh>

# include <reactor/fwd.hh>

# if defined(INFINIT_MACOSX) or defined(INFINIT_WINDOWS)
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

    /// Function for sending existing crash reports
    void
    existing_report(std::string const& host,
                    uint16_t port,
                    std::string const& user_name = "",
                    std::string const& os_description = "",
                    std::string const& info = "",
                    std::string const& file = "");

    /// Function for sending user reports
    void
    user_report(std::string const& host,
                uint16_t port,
                std::string const& user_name = "",
                std::string const& os_description = "",
                std::string const& message = "",
                std::string const& file = "");
  };
}

#endif
