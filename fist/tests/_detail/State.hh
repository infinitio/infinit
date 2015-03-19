#ifndef FIST_SURFACE_GAP_TESTS_STATE_HH
# define FIST_SURFACE_GAP_TESTS_STATE_HH

# include <elle/filesystem/TemporaryDirectory.hh>
# include <elle/UUID.hh>

# include <surface/gap/State.hh>

namespace tests
{
  class Server;

  class State
  {
  public:
    // XXX Dirty hack.
    surface::gap::State* operator ->() { return &this->_state; };
    State(Server& server,
          elle::UUID device_id,
          boost::filesystem::path const& home = boost::filesystem::path());

    ELLE_ATTRIBUTE(elle::filesystem::TemporaryDirectory, temporary_dir);
    ELLE_ATTRIBUTE_X(surface::gap::State, state);
  };
}

#endif
