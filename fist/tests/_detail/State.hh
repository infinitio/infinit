#ifndef FIST_SURFACE_GAP_TESTS_STATE_HH
# define FIST_SURFACE_GAP_TESTS_STATE_HH

# include <surface/gap/State.hh>

# include <fist/tests/_detail/uuids.hh>

namespace tests
{
  class Server;

  class State
    : public surface::gap::State
  {
  public:
    State(Server& server,
          boost::uuids::uuid device_id,
          boost::filesystem::path const& path = boost::filesystem::path());
  };
}

#endif
