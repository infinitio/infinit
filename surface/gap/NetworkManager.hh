#ifndef NETWORKMANAGER_HH
# define NETWORKMANAGER_HH

# include <plasma/meta/Client.hh>
# include <surface/gap/NotificationManager.hh>
# include <surface/gap/Exception.hh>
# include <surface/gap/_detail/InfinitInstanceManager.hh>

# include <nucleus/neutron/Permissions.hh>

# include <reactor/scheduler.hh>

namespace surface
{
  namespace gap
  {
    using Self = ::plasma::meta::SelfResponse;
    using Network = ::plasma::meta::NetworkResponse;
    using Device = ::plasma::meta::Device;
    using Endpoint = ::plasma::meta::EndpointNodeResponse;

    class NetworkManager
    {
      class Exception:
        surface::gap::Exception
      {
      public:
        Exception(gap_Status error, std::string const& what):
          surface::gap::Exception{error, what}
        {}

        Exception(std::string const& what):
          surface::gap::Exception{gap_error, what}
        {}
      };

    private:
      // XXX: meta should be constant everywhere.
      // But httpclient fire can't be constant.
      plasma::meta::Client& _meta;
      Self const& _self;
      Device const& _device;
      ELLE_ATTRIBUTE_R(InfinitInstanceManager, infinit_instance_manager);

    public:
      NetworkManager(plasma::meta::Client& meta,
                     Self const& me,
                     Device const& device);

      virtual
      ~NetworkManager();

      void
      clear();

    public:
      bool
      wait_portal(std::string const& network_id);

      /// Types.
    protected:
      using Network = ::plasma::meta::NetworkResponse;
      typedef std::unique_ptr<Network> NetworkPtr;
      typedef std::map<std::string, NetworkPtr> NetworkMap;
    protected:

      std::unique_ptr<NetworkMap> _networks;

    public:
      /// Retrieve all networks.
      NetworkMap const&
      all();

      /// Retrieve a network.
      Network&
      one(std::string const& id);

      Network&
      sync(std::string const& id);

      /// Create a new network.
      std::string
      create(std::string const& name,
             bool auto_add = true);

      /// Prepare directories and files for the network to be launched.
      void
      prepare(std::string const& network_id);

      /// Delete a new network.
      std::string
      delete_(std::string const& name,
              bool force = false);

      /// Add a user to a network with its mail or id.
      void
      add_user(std::string const& network_id,
               std::string const& inviter_id,
               std::string const& user_id,
               std::string const& identity);

      ///
      void
      _prepare_directory(std::string const& network_id);


      /// XXX
      void
      notify_8infinit(std::string const& network_id,
                      std::string const& sender_device_id,
                      std::string const& recipient_device_id,
                      reactor::Scheduler& sched);

      /// Give the recipient the write on the root of the network.
      void
      set_permissions(std::string const& network_id,
                      std::string const& user_id,
                      std::string const& user_identity,
                      nucleus::neutron::Permissions permissions);

      /*/// Set the permissions for a file.
      /// XXX: old
      void
      deprecated_set_permissions(std::string const& user_id,
                                 std::string const& abspath,
                                 nucleus::neutron::Permissions permissions,
                                 bool recursive = false);
      */


      /// On NetworkUpdate.
      void
      _on_network_update(NetworkUpdateNotification const& notif);

    };
  }
}

#endif
