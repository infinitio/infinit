#include <surface/gap/Rounds.hh>

namespace surface
{
  namespace gap
  {
    round::round():
      _endpoints{},
      _name{}
    {}

    round::round(round&& r):
      _endpoints{std::move(r._endpoints)},
      _name{std::move(r._name)}
    {}

    round::round(round const& r):
      _endpoints{r._endpoints},
      _name{r._name}
    {
    }
  }
}
