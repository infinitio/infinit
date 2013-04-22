#ifndef  SURFACE_GAP_INFINITINSTANCEMANAGER_HH
# define SURFACE_GAP_INFINITINSTANCEMANAGER_HH

# include <elle/system/Process.hh>
# include <surface/gap/Exception.hh>

# include <map>
# include <memory>
# include <string>

namespace surface
{
  namespace gap
  {

    typedef std::unique_ptr<elle::system::Process> SystemProcessPtr;

    struct InfinitInstance
    {
      std::string       network_id;
      std::string       mount_point;
      SystemProcessPtr  process;
    };

    class InfinitInstanceManager
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
      std::string _user_id;

    private:
      typedef std::unique_ptr<InfinitInstance> InfinitInstancePtr;
      std::map<std::string, InfinitInstancePtr> _instances;

    public:
      explicit
      InfinitInstanceManager(std::string const& user_id);

    public:
      virtual
      ~InfinitInstanceManager();

      void
      clear();

      bool
      wait_portal(std::string const& network_id);

      void
      launch(std::string const& network_id);

      bool
      exists(std::string const& network_id) const;

      void
      stop(std::string const& network_id);

      InfinitInstance const&
      instance(std::string const& network_id) const;

      InfinitInstance const*
      instance_for_file(std::string const& path);
    };

  }
}

#endif
