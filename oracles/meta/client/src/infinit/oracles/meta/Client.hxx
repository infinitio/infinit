#ifndef PLASMA_META_CLIENT_HXX
# define PLASMA_META_CLIENT_HXX

# include <infinit/oracles/meta/Client.hh>

namespace std
{
  template<>
  struct hash<infinit::oracles::meta::User>
  {
  public:
    std::size_t
    operator()(infinit::oracles::meta::User const& user) const
    {
      return std::hash<std::string>()(user.id);
    }
  };
}

#endif
