#include <nucleus/proton/Handle.hh>
#include <nucleus/proton/Porcupine.hh>

#include <cryptography/SecretKey.hh>

namespace nucleus
{
  namespace proton
  {
    /*-------------.
    | Construction |
    `-------------*/

    Handle::Handle():
      _phase(Phase::unnested),
      _clef(nullptr)
    {
      // Manually set all the union pointers to null so as to make sure all
      // the cases are handled.
      this->_clef = nullptr;
      this->_egg = nullptr;
    }

    Handle::Handle(Address const& address,
                   cryptography::SecretKey const& secret):
      _phase(Phase::unnested),
      _clef(new Clef{address, secret})
    {
    }

    Handle::Handle(std::shared_ptr<Egg>& egg):
      _phase(Phase::nested),
      _egg(new std::shared_ptr<Egg>{egg})
    {
    }

    Handle::Handle(Handle const& other):
      _phase(other._phase)
    {
      // Manually set all the union pointers to null so as to make sure all
      // the cases are handled.
      this->_clef = nullptr;
      this->_egg = nullptr;

      switch (this->_phase)
        {
        case Phase::unnested:
          {
            ELLE_ASSERT(other._clef != nullptr);

            this->_clef = new Clef{*other._clef};

            break;
          }
        case Phase::nested:
          {
            ELLE_ASSERT(other._egg != nullptr);
            ELLE_ASSERT(*other._egg != nullptr);

            this->_egg = new std::shared_ptr<Egg>{*other._egg};

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown phase '%s'", this->_phase));
        }
    }

    Handle::~Handle()
    {
      switch (this->_phase)
        {
        case Phase::unnested:
          {
            delete this->_clef;

            break;
          }
        case Phase::nested:
          {
            break;
          }
        default:
          throw Exception(elle::sprintf("unknown phase '%s'", this->_phase));
        }
    }

    /*--------.
    | Methods |
    `--------*/

    Address const&
    Handle::address() const
    {
      switch (this->_phase)
        {
        case Phase::unnested:
          {
            ELLE_ASSERT(this->_clef != nullptr);

            return (this->_clef->address());
          }
        case Phase::nested:
          {
            ELLE_ASSERT(this->_egg != nullptr);
            ELLE_ASSERT(*this->_egg != nullptr);

            return ((*this->_egg)->address());
          }
        default:
          throw Exception(elle::sprintf("unknown phase '%s'", this->_phase));
        }
    }

    cryptography::SecretKey const&
    Handle::secret() const
    {
      switch (this->_phase)
        {
        case Phase::unnested:
          {
            ELLE_ASSERT(this->_clef != nullptr);

            return (this->_clef->secret());
          }
        case Phase::nested:
          {
            ELLE_ASSERT(this->_egg != nullptr);
            ELLE_ASSERT(*this->_egg != nullptr);

            return ((*this->_egg)->secret());
          }
        default:
          throw Exception(elle::sprintf("unknown phase '%s'", this->_phase));
        }
    }

    std::shared_ptr<Egg> const&
    Handle::egg() const
    {
      ELLE_ASSERT(this->_phase == Phase::nested);
      ELLE_ASSERT(this->_egg != nullptr);
      ELLE_ASSERT(*this->_egg != nullptr);

      return (*this->_egg);
    }

    std::shared_ptr<Egg>&
    Handle::egg()
    {
      ELLE_ASSERT(this->_phase == Phase::nested);
      ELLE_ASSERT(this->_egg != nullptr);
      ELLE_ASSERT(*this->_egg != nullptr);

      return (*this->_egg);
    }

    void
    Handle::place(std::shared_ptr<Egg>& egg)
    {
      ELLE_ASSERT(this->_phase == Phase::unnested);

      // Delete the previous clef.
      ELLE_ASSERT(this->_clef != nullptr);
      delete this->_clef;
      this->_clef = nullptr;

      // Set the egg to reference to access the block.
      this->_egg = new std::shared_ptr<Egg>{egg};

      // Update the phase.
      this->_phase = Phase::nested;
    }

    void
    Handle::evolve()
    {
      ELLE_ASSERT(this->_phase == Phase::unnested);

      // Keep the clef's memory address and reset the attribute.
      ELLE_ASSERT(this->_clef != nullptr);
      Clef* clef = this->_clef;
      this->_clef = nullptr;

      // Allocate a new egg based on the handle's clef.
      this->_egg = new std::shared_ptr<Egg>{new Egg{clef}};

      // Update the phase.
      this->_phase = Phase::nested;
    }

    void
    Handle::reset(Address const& address,
                  cryptography::SecretKey const& secret)
    {
      switch (this->_phase)
        {
        case Phase::unnested:
          {
            ELLE_ASSERT(this->_clef != nullptr);

            // In this case, regenerate the clef.
            delete this->_clef;
            this->_clef = nullptr;
            this->_clef = new Clef{address, secret};

            break;
          }
        case Phase::nested:
          {
            ELLE_ASSERT(this->_egg != nullptr);
            ELLE_ASSERT(*this->_egg != nullptr);

            // In this case, reset the egg.
            (*this->_egg)->reset(address, secret);

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown phase '%s'", this->_phase));
        }
    }

    State
    Handle::state() const
    {
      // Depending on the phase which is nested or not.
      switch (this->_phase)
      {
        case Handle::Phase::unnested:
        {
          // If the handle is unnested, it means the block has never been loaded
          // in which case it is, by definition, clean.
          return (State::clean);
        }
        case Handle::Phase::nested:
        {
          // Otherwise, the egg knows the state of the block.
          ELLE_ASSERT_NEQ(this->_egg, nullptr);

          return ((*this->_egg)->state());
        }
        default:
          throw Exception(
            elle::sprintf("unknown handle phase '%s'", this->_phase));
      }

      elle::unreachable();
    }

    void
    Handle::state(State const state)
    {
      ELLE_ASSERT_NEQ(this->_egg, nullptr);

      // Update the egg's state.
      (*this->_egg)->state(state);
    }

    /*----------.
    | Operators |
    `----------*/

    elle::Boolean
    Handle::operator ==(Handle const& other) const
    {
      if (this == &other)
        return true;

      // Compare the handles depending on their phase.
      if ((this->_phase == Phase::nested) &&
          (other._phase == Phase::nested))
        {
          // In this case, both handles reference a transient block through
          // an egg whose memory address can be compared to know if both track
          // the same block.
          ELLE_ASSERT(this->_egg != nullptr);
          ELLE_ASSERT(other._egg != nullptr);
          ELLE_ASSERT(*this->_egg != nullptr);
          ELLE_ASSERT(*other._egg != nullptr);

          return ((*this->_egg).get() == (*other._egg).get());
        }
      else
        {
          // Otherwise, one of the two handles represent a block which
          // has not yet been retrieved from the network. This block is
          // therefore permanent i.e has an address.
          //
          // Since transient blocks have a temporary address (until they get
          // bound), it would not be wise to base the comparison on it
          // as, if being extremely unlucky, the temporary address could
          // end up being the same as another permanent block.
          //
          // The following therefore considers that if one of the block is
          // transient (this other cannot be otherwise the 'if' case would
          // have been true), then the handles reference different blocks.
          //
          // Otherwise, both blocks are permanent in which case their addresses
          // can be compared, no matter the phase of the handle.

          // Check whether one of the handles reference a nested and transient
          // block.
          if (((this->_phase == Phase::nested) &&
               ((*this->_egg)->type() == Egg::Type::transient)) ||
              ((other._phase == Phase::nested) &&
               ((*other._egg)->type() == Egg::Type::transient)))
            return (false);

          // At this point we know that both blocks are permanent being unnested
          // or nested; their addresses can therefore be compared.
          return (this->address() == other.address());
        }

      elle::unreachable();
    }

    /*---------.
    | Dumpable |
    `---------*/

    elle::Status
    Handle::Dump(const elle::Natural32      margin) const
    {
      elle::String alignment(margin, ' ');

      std::cout << alignment << "[Handle] " << this->_phase << std::endl;

      switch (this->_phase)
        {
        case Phase::unnested:
          {
            ELLE_ASSERT(this->_clef != nullptr);

            std::cout << alignment << elle::io::Dumpable::Shift
                      << "[Clef] " << *this->_clef << std::endl;

            break;
          }
        case Phase::nested:
          {
            ELLE_ASSERT(this->_egg != nullptr);
            ELLE_ASSERT(*this->_egg != nullptr);

            std::cout << alignment << elle::io::Dumpable::Shift
                      << "[Egg] " << *(*this->_egg) << std::endl;

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown phase '%s'", this->_phase));
        }

      return elle::Status::Ok;
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Handle::print(std::ostream& stream) const
    {
      stream << this->_phase << "(";

      switch (this->_phase)
        {
        case Phase::unnested:
          {
            ELLE_ASSERT(this->_clef != nullptr);

            stream << *this->_clef;

            break;
          }
        case Phase::nested:
          {
            ELLE_ASSERT(this->_egg != nullptr);
            ELLE_ASSERT(*this->_egg != nullptr);

            stream << *(*this->_egg);

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown phase '%s'", this->_phase));
        }

      stream << ")";
    }

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Handle::Phase const phase)
    {
      switch (phase)
        {
        case Handle::Phase::unnested:
          {
            stream << "unnested";
            break;
          }
        case Handle::Phase::nested:
          {
            stream << "nested";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown phase: '%s'",
                                          static_cast<int>(phase)));
          }
        }

      return (stream);
    }
  }
}
