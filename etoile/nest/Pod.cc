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
      _attachment(Attachment::attached),
      _state(State::dangling),
      _actors(0),
      _egg(egg),
      _position(position)
    {
    }

    Pod::Pod(std::shared_ptr<nucleus::proton::Egg>&& egg,
             std::list<Pod*>::iterator const& position):
      _attachment(Attachment::attached),
      _state(State::dangling),
      _actors(0),
      _egg(std::move(egg)),
      _position(position)
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
             << "(" << this->_attachment << ", " << this->_state << ", "
             << this->_actors << ", "
             << (this->_mutex.locked() == true ? "locked" : "unlocked")
             << ")";
    }

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Pod::Attachment const attachment)
    {
      switch (attachment)
        {
        case Pod::Attachment::attached:
          {
            stream << "attached";
            break;
          }
        case Pod::Attachment::detached:
          {
            stream << "detached";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown attachment: '%s'",
                                          static_cast<int>(attachment)));
          }
        }

      return (stream);
    }

    std::ostream&
    operator <<(std::ostream& stream,
                Pod::State const state)
    {
      switch (state)
      {
        case Pod::State::dangling:
        {
          stream << "dangling";
          break;
        }
        case Pod::State::use:
        {
          stream << "use";
          break;
        }
        case Pod::State::queue:
        {
          stream << "queue";
          break;
        }
        case Pod::State::shell:
        {
          stream << "shell";
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
