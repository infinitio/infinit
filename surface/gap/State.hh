#ifndef  SURFACE_GAP_STATE_HH
# define SURFACE_GAP_STATE_HH

# include "Device.hh"
# include "Exception.hh"
# include "NetworkManager.hh"
# include "NotificationManager.hh"
# include "Self.hh"
# include "TransactionManager.hh"
# include "UserManager.hh"
# include "gap.h"
# include "metrics.hh"

# include <metrics/Reporter.hh>

# include <common/common.hh>

# include <plasma/meta/Client.hh>
# include <plasma/trophonius/Client.hh>

# include <elle/format/json/fwd.hh>
# include <elle/threading/Monitor.hh>
# include <elle/Printable.hh>

# include <map>
# include <string>
# include <exception>

namespace surface
{
  namespace gap
  {
    class TransferMachine;

    struct FileInfos
    {
      std::string                 mount_point;
      std::string                 network_id;
      std::string                 absolute_path;
      std::string                 relative_path;
      std::map<std::string, int>  accesses;
    };

    // Used to represent all users in the state class.
    using Nodes = ::plasma::meta::NetworkNodesResponse;
    using Network = ::plasma::meta::NetworkResponse;
    using ::plasma::Transaction;

    // XXX: In order to ensure the logger is initialized at the begining of
    // state LoggerInitializer MUST be the first member of State.
    class LoggerInitializer
    {
    public:
      LoggerInitializer();
    };

    class State:
      public elle::Printable
    {
    public:
      ///- Logs ----------------------------------------------------------------
      // XXX: LoggerInitializer is the first member of state.
      LoggerInitializer _logger_intializer;

      ///- Servers -------------------------------------------------------------
      ELLE_ATTRIBUTE_R(plasma::meta::Client, meta);

      ELLE_ATTRIBUTE_X(metrics::Reporter, reporter);
      ELLE_ATTRIBUTE(metrics::Reporter, google_reporter);

      ///- Scheduler -----------------------------------------------------------
      ELLE_ATTRIBUTE_R(reactor::Scheduler, scheduler);
      ELLE_ATTRIBUTE_R(reactor::Thread, keep_alive);
      ELLE_ATTRIBUTE_R(std::thread, thread);
      ELLE_ATTRIBUTE(std::exception_ptr, exception);

      ///- Construction --------------------------------------------------------
    public:
      State(std::string const& meta_host = common::meta::host(),
            uint16_t meta_port = common::meta::port(),
            std::string const& trophonius_host = common::trophonius::host(),
            uint16_t trophonius_port = common::trophonius::port(),
            std::string const& apertus_host = common::apertus::host(),
            uint16_t apertus_port = common::apertus::port());
      ~State();

    public:
      bool
      logged_in() const
      {
        return !this->_meta.token().empty();
      }

    //- Login & register ------------------------------------------------------
      std::unique_ptr<Self> mutable _me;

      Self const&
      me() const;
    public:
      /// Login to meta.
      void
      login(std::string const& email,
            std::string const& password);

      /// Logout from meta.
      void
      logout();

      void
      register_(std::string const& fullname,
                std::string const& email,
                std::string const& password,
                std::string const& activation_code);

      ///
      std::string
      hash_password(std::string const& email,
                    std::string const& password);

      std::string
      user_directory();

      /// Retrieve current user token.
      std::string const&
      token();

      std::string const&
      token_generation_key() const;

      void
      send_files(std::string const& recipient,
                 std::unordered_set<std::string>&& files);

    private:
      ELLE_ATTRIBUTE_P(std::unique_ptr<Device>, device, mutable);

    public:
      Device const&
      device() const;
      std::string const&
      device_id() const;
      std::string const&
      device_name() const;

      ELLE_ATTRIBUTE_P(elle::Passport, passport, mutable);
      ELLE_ATTRIBUTE_P(lune::Identity, identity, mutable);
    public:
      elle::Passport const&
      passport() const;

      lune::Identity const&
      identity() const;


    ///
    /// Manage local device.
    ///
    public:
      /// Check if the local device has been created.
      bool
      has_device() const;

      /// Create or update the local device.
      void
      update_device(std::string const& name,
                    bool force_create = false) const;

    private:
      ELLE_ATTRIBUTE_R(std::string, trophonius_host);
      ELLE_ATTRIBUTE_R(uint16_t, trophonius_port);

      typedef std::unique_ptr<NotificationManager> NotificationManagerPtr;
      elle::threading::Monitor<NotificationManagerPtr> _notification_manager;

      ELLE_ATTRIBUTE_R(std::string, apertus_host);
      ELLE_ATTRIBUTE_R(uint16_t, apertus_port);

      typedef std::unique_ptr<NetworkManager> NetworkManagerPtr;
      elle::threading::Monitor<NetworkManagerPtr> _network_manager;

      typedef std::unique_ptr<UserManager> UserManagerPtr;
      elle::threading::Monitor<UserManagerPtr> _user_manager;

      typedef std::unique_ptr<TransactionManager> TransactionManagerPtr;
      elle::threading::Monitor<TransactionManagerPtr> _transaction_manager;

      typedef std::unique_ptr<TransferMachine> TransferMachinePtr;
      std::vector<TransferMachinePtr> _transfers;

    public:
      NetworkManager&
      network_manager();

      NotificationManager&
      notification_manager();

      UserManager&
      user_manager();

      TransactionManager&
      transaction_manager();

    private:

      void
      _cleanup();

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;
    };

  }
}


#endif
