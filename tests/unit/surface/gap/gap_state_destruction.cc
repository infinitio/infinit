#include <iostream>
#include <stdexcept>
#include <memory>

#include <lune/Lune.hh>
#include <surface/gap/gap.h>
#include <version.hh>

namespace std
{
  template <>
  struct default_delete<gap_State>
  {
    void
    operator ()(gap_State* ptr) const
    {
      gap_free(ptr);
    }
  };
}

int
main(int argc, char** argv)
{
  try
  {
    lune::Lune::Initialize(); //XXX

    std::unique_ptr<gap_State> state{gap_new()};

    gap_login(state.get(), "wrongemail.com", "fakepassword");
  }
  catch (std::runtime_error const& e)
  {
    std::cerr << argv[0] << ": " << e.what() << "." << std::endl;
    return 1;
  }
  return 0;
}
