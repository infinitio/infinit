#ifndef  SURFACE_GAP_INFINITINSTANCEMANAGER_HH
# define SURFACE_GAP_INFINITINSTANCEMANAGER_HH

# include <map>
# include <memory>
# include <unordered_set>
# include <string>

# include <elle/serialize/extract.hh>
# include <elle/system/Process.hh>

# include <reactor/scheduler.hh>
# include <reactor/thread.hh>

# include <Infinit.hh>
# include <common/common.hh>
# include <etoile/Etoile.hh>
# include <hole/Hole.hh>
# include <hole/implementations/slug/Slug.hh>
# include <hole/storage/Directory.hh>
# include <lune/Identity.hh>
# include <lune/Descriptor.hh>
# include <nucleus/neutron/Object.hh>
# include <nucleus/proton/Network.hh>
# include <surface/gap/Exception.hh>

namespace surface
{
  namespace gap
  {
    /*------.
    | Types |
    `------*/
    typedef std::unique_ptr<elle::system::Process> SystemProcessPtr;

    struct InfinitInstance
    {
      // XXX: we might want to keep the passport/identity/... in the manager
      // directly.
      std::string network_id;
      std::string mount_point;
      nucleus::proton::Network network;
      lune::Identity identity;
      elle::Passport passport;
      lune::Descriptor descriptor;
      hole::storage::Directory storage;
      std::unique_ptr<hole::Hole> hole;
      std::unique_ptr<etoile::Etoile> etoile;
      reactor::Scheduler scheduler;
      reactor::Thread keep_alive;
      std::thread thread;

      InfinitInstance(std::string const& user_id,
                      std::string const& network_id,
                      lune::Identity const& identity,
                      std::string const& descriptor); //XXX: Should be movable.
    };

    class InfinitInstanceManager:
      public elle::Printable
    {
      /*----------.
      | Exception |
      `----------*/
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

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      typedef std::unique_ptr<InfinitInstance> InfinitInstancePtr;
      std::map<std::string, InfinitInstancePtr> _instances;
      std::string _user_id;

      /*-------------.
      | Construction |
      `-------------*/
    public:
      explicit
      InfinitInstanceManager(std::string const& user_id);

    public:
      virtual
      ~InfinitInstanceManager();

      void
      clear();

      // Change descriptor_digest that to lune::Descriptor&& as soon as
      // lune::Descriptor is movable.
      void
      launch(std::string const& network_id,
             lune::Identity const& identity,
             std::string const& descriptor_digest);

      bool
      exists(std::string const& network_id) const;

      void
      stop(std::string const& network_id);

      void
      add_user(std::string const& network_id,
               nucleus::neutron::Group::Identity const& group,
               nucleus::neutron::Subject const& subject);

      void
      grant_permissions(std::string const& network_id,
                        nucleus::neutron::Subject const& subject);

      void
      upload_files(std::string const& network_id,
                   std::unordered_set<std::string> items);

    private:
      InfinitInstance&
      _instance(std::string const& network_id);

      InfinitInstance const*
      _instance_for_file(std::string const& path);

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
