#ifndef TRANSFERMACHINE_HXX
# define TRANSFERMACHINE_HXX

namespace std
{
  template<>
  struct hash<surface::gap::TransferMachine>
  {
  public:
    std::size_t operator()(surface::gap::TransferMachine const& tm) const
    {
      return (size_t) tm.id;
    }
  };
}


#endif
