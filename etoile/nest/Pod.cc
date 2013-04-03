#include <etoile/nest/Pod.hh>
#include <etoile/Exception.hh>

#include <nucleus/proton/Egg.hh>

namespace etoile
{
  namespace nest
  {
    /*-------------.
    | Construction |
    `-------------*/

    Pod::Pod(std::shared_ptr<nucleus::proton::Egg>& egg,
             std::list<Pod*>::iterator const& position):
      _state(State::attached),
      _actors(0),
      _egg(egg),
      _position(position),
      _footprint(0)
    {
    }

    Pod::Pod(std::shared_ptr<nucleus::proton::Egg>&& egg,
             std::list<Pod*>::iterator const& position):
      _state(State::attached),
      _actors(0),
      _egg(std::move(egg)),
      _position(position),
      _footprint(0)
    {
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Pod::print(std::ostream& stream) const
    {
      ELLE_ASSERT(this->_egg != nullptr);

      stream << *this->_egg
             << "(" << this->_state << ", " << this->_actors << ", "
             << this->_footprint << ", "
             << (this->_mutex.locked() == true ? "locked" : "unlocked")
             << ")";
    }

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Pod::State const state)
    {
      switch (state)
        {
        case Pod::State::attached:
          {
            stream << "attached";
            break;
          }
        case Pod::State::detached:
          {
            stream << "detached";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown state: '%s'",
                                          static_cast<int>(state)));
          }
        }

      return (stream);
    }
  }
}
