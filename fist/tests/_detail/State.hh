#ifndef FIST_SURFACE_GAP_TESTS_STATE_HH
# define FIST_SURFACE_GAP_TESTS_STATE_HH

# include <elle/UUID.hh>

# include <surface/gap/State.hh>

namespace tests
{
  class Server;

  class State
    : public surface::gap::State
  {
  public:
    State(Server& server,
          elle::UUID device_id,
          boost::filesystem::path const& path = boost::filesystem::path());

    void
    synchronize();
  };
}

#endif
