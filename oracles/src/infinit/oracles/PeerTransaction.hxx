#ifndef INFINIT_ORACLES_PEER_TRANSACTION_HXX
# define INFINIT_ORACLES_PEER_TRANSACTION_HXX

namespace std
{
  template<>
  struct hash<infinit::oracles::PeerTransaction>
  {
  public:
    std::size_t operator()(infinit::oracles::PeerTransaction const& tr) const
    {
      return std::hash<std::string>()(tr.id);
    }
  };
}

#endif
