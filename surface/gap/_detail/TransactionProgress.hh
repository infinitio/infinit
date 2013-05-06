#ifndef SURFACE_GAP_TRANSACTIONPROGRESS_HH
# define SURFACE_GAP_TRANSACTIONPROGRESS_HH

# include "../State.hh"

# include <elle/system/Process.hh>

# include <memory>

namespace surface
{
  namespace gap
  {
    struct State::TransactionProgress
    {
    public:
      float last_value;
      std::unique_ptr<elle::system::Process> process;

    public:
      TransactionProgress():
        last_value{0.0f}
      {}
    };
  }
}

#endif
