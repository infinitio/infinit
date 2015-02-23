#ifndef ELLE_CRASHREPORTER_HH
# define ELLE_CRASHREPORTER_HH

# include <cstring>

# include <boost/noncopyable.hpp>
# include <boost/filesystem/path.hpp>

# include <elle/Backtrace.hh>

# include <reactor/fwd.hh>

# if defined(INFINIT_MACOSX) or defined(INFINIT_WINDOWS)
  typedef void(*sighandler_t)(int);
# else
#  include <signal.h>
# endif

namespace elle
{
#ifndef INFINIT_IOS
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

      ELLE_ATTRIBUTE_R(boost::filesystem::path, home);
    };
  } // End of signal.
#endif // INFINIT_IOS

  namespace crash
  {
    /// Function for sending existing crash reports.
    void
    existing_report(std::string const& meta_protocol,
                    std::string const& meta_host,
                    uint16_t meta_port,
                    std::vector<std::string> const& files,
                    std::string const& user_name = "",
                    std::string const& info = "");

    /// Function for sending a report when a transfer fails.
    void
    transfer_failed_report(std::string const& meta_protocol,
                           std::string const& meta_host,
                           uint16_t meta_port,
                           boost::filesystem::path const& attachment,
                           std::string const& user_name="",
                           std::string const& transaction_id="",
                           std::string const& reason="");

    /// Function for sending user reports.
    void
    user_report(std::string const& meta_protocol,
                std::string const& meta_host,
                uint16_t meta_port,
                boost::filesystem::path const& attachment,
                std::string const& user_name = "",
                std::string const& message = "",
                std::string const& user_file = "");

  }
}

#endif
