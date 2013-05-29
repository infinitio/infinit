#ifndef  SURFACE_GAP_STATE_HH
# define SURFACE_GAP_STATE_HH

# include "gap.h"

# include <surface/gap/Exception.hh>
# include <surface/gap/NetworkManager.hh>
# include <surface/gap/NotificationManager.hh>
# include <surface/gap/UserManager.hh>
# include <surface/gap/TransactionManager.hh>

# include <surface/gap/metrics.hh>

# include <plasma/meta/Client.hh>
# include <plasma/trophonius/Client.hh>

# include <elle/format/json/fwd.hh>
# include <elle/threading/Monitor.hh>

# include <map>
# include <string>


namespace surface
{
  namespace gap
  {
    using Self = ::plasma::meta::SelfResponse;
    using Device = ::plasma::meta::Device;

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

    class State
    {
    public:
      ///- Logs ----------------------------------------------------------------
      // XXX: LoggerInitializer is the first member of state.
      LoggerInitializer _logger_intializer;

      ///- Servers -------------------------------------------------------------
      plasma::meta::Client _meta;

      ELLE_ATTRIBUTE_X(elle::metrics::Reporter, reporter);
      ELLE_ATTRIBUTE(elle::metrics::Reporter, google_reporter);

      ///- Construction --------------------------------------------------------
    public:
      State();
      ~State();

    public:
      bool
      logged_in() const
      {
        return !this->_meta.token().empty();
      }

    //- Login & register ------------------------------------------------------
      std::unique_ptr<Self> mutable _me;

      Self&
      me();

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

    private:
      std::unique_ptr<Device> _device;

    public:
      Device&
      device();
      std::string const&
      device_id();
      std::string const&
      device_name();

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
                    bool force_create = false);
    ///
    /// File infos
    ///
    private:
      std::map<std::string, FileInfos*> _files_infos;
    public:
      /// Retrieve files infos.
      // FileInfos const&
      // file_infos(std::string const& abspath);

      /// Get size of a given path.
      size_t
      file_size(std::string const& path);

      std::string
      file_name(std::string const& path);

    private:
      typedef std::unique_ptr<NetworkManager> NetworkManagerPtr;
      elle::threading::Monitor<NetworkManagerPtr> _network_manager;

      typedef std::unique_ptr<NotificationManager> NotificationManagerPtr;
      elle::threading::Monitor<NotificationManagerPtr> _notification_manager;

      typedef std::unique_ptr<UserManager> UserManagerPtr;
      elle::threading::Monitor<UserManagerPtr> _user_manager;

      typedef std::unique_ptr<TransactionManager> TransactionManagerPtr;
      elle::threading::Monitor<TransactionManagerPtr> _transaction_manager;

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
    };

  }
}


#endif
