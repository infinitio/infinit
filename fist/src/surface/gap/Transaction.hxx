#ifndef SURFACE_GAP_TRANSACTION_HXX
# define SURFACE_GAP_TRANSACTION_HXX

# include <elle/serialize/SetSerializer.hxx>

namespace std
{
  template<>
  struct hash<surface::gap::Transaction>
  {
    size_t
    operator ()(surface::gap::Transaction const& tr)
    {
      return std::hash<std::string>()(tr.data()->id);
    }
  };
}

#endif
