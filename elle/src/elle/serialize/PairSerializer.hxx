#ifndef ELLE_SERIALIZE_PAIRSERIALIZER_HXX
# define ELLE_SERIALIZE_PAIRSERIALIZER_HXX

# include <utility>

# include "Serializer.hh"

namespace elle
{
  namespace serialize
  {

    template <typename First, typename Second>
    struct Serializer<std::pair<First, Second>>:
      public SplitSerializer<std::pair<First, Second>>
    {
      template <typename Archive>
      static
      void
      Load(Archive& ar,
           std::pair<First, Second>& pair,
           unsigned int)
      {
        ar >> pair.first;
        ar >> pair.second;
      }

      template <typename Archive>
      static
      void
      Save(Archive& ar,
           std::pair<First, Second> const& pair,
           unsigned int)
      {
        ar << pair.first;
        ar << pair.second;
      }
    };

    template <typename First, typename Second>
    struct StoreFormat<std::pair<First, Second>>
    {
      static bool const value = false;
    };

  }
}

#endif
