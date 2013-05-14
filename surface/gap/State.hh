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

      elle::metrics::Reporter _reporter;
      elle::metrics::Reporter _google_reporter;

    public:
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
      Self _me;
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

      /// Retrieve current user data.
      Self const& me();

    private:
      Device _device;

    public:
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
      std::unique_ptr<NetworkManager> _network_manager;

    public:
      NetworkManager&
      network_manager()
      {
        if (this->_network_manager == nullptr)
        {
          this->_network_manager.reset(
            new NetworkManager{this->_meta,
                               this->_reporter,
                               this->_google_reporter,
                               this->_me,
                               this->_device});
        }
        return *this->_network_manager;
      }

    private:
      std::unique_ptr<NotificationManager> _notification_manager;

    public:
      NotificationManager&
      notification_manager()
      {
        if (this->_notification_manager == nullptr)
        {
          this->_notification_manager.reset(new NotificationManager{this->_meta,
                                                                    this->_me});
        }
        return *this->_notification_manager;
      }

    private:
      std::unique_ptr<UserManager> _user_manager;

    public:
      UserManager&
      user_manager()
      {
        if (this->_user_manager == nullptr)
        {
          this->_user_manager.reset(
            new UserManager{this->notification_manager(),
                            this->_meta,
                            this->_me});
        }
        return *this->_user_manager;
      }

    private:
      std::unique_ptr<TransactionManager> _transaction_manager;
    public:
      TransactionManager&
      transaction_manager()
      {
        if (this->_transaction_manager == nullptr)
        {
          this->_transaction_manager.reset(
            new TransactionManager{this->notification_manager(),
                                   this->network_manager(),
                                   this->user_manager(),
                                   this->_meta,
                                   this->_reporter,
                                   this->_me,
                                   this->_device});
        }
        return *this->_transaction_manager;
      }

    private:

      void
      _cleanup_managers();
    };

  }
}


#endif
