#include <reactor/scheduler.hh>

#include <nucleus/proton/Egg.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Contents.hh>
#include <nucleus/proton/Clef.hh>
#include <nucleus/Exception.hh>

#include <cryptography/SecretKey.hh>

namespace nucleus
{
  namespace proton
  {
    /*-------------.
    | Construction |
    `-------------*/

    Egg::Egg(Address const& address,
             cryptography::SecretKey const& secret):
      _type(Type::permanent),
      _alive(new Clef{address, secret})
    {
    }

    Egg::Egg(Clef const* clef):
      _type(Type::permanent),
      _alive(clef)
    {
    }

    Egg::Egg(Contents* block,
             Address const& address,
             cryptography::SecretKey const& secret):
      _type(Type::transient),
      _alive(new Clef{address, secret}),
      _block(block)
    {
    }

    /*--------.
    | Methods |
    `--------*/

    elle::Boolean
    Egg::has_history() const
    {
      return (this->_historical != nullptr);
    }

    Address const&
    Egg::address() const
    {
      ELLE_ASSERT(this->_alive != nullptr);

      return (this->_alive->address());
    }

    cryptography::SecretKey const&
    Egg::secret() const
    {
      ELLE_ASSERT(this->_alive != nullptr);

      return (this->_alive->secret());
    }

    Clef const&
    Egg::alive() const
    {
      ELLE_ASSERT(this->_alive != nullptr);

      return (*this->_alive);
    }

    Clef const&
    Egg::historical() const
    {
      ELLE_ASSERT(this->_historical != nullptr);

      return (*this->_historical);
    }

    void
    Egg::reset(Address const& address,
               cryptography::SecretKey const& secret)
    {
      // Update the type and history according to the current type.
      switch (this->_type)
        {
        case Type::transient:
          {
            // In this case, upgrade the transient block to a permanent
            // one now that it has a valid address and secret.
            this->_type = Type::permanent;

            break;
          }
        case Type::permanent:
          {
            // In this case, the egg must keep track of the block's previous
            // address/secret tuple so as to be able to access this version,
            // to wipe it from the storage layer for instance.
            this->_historical = std::move(this->_alive);

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown egg type '%s'", this->_type));
        }

      // Regenerate the clef with the new address and secret.
      this->_alive.reset(new Clef{address, secret});
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Egg::print(std::ostream& stream) const
    {
      ELLE_ASSERT(this->_alive != nullptr);

      stream << this->_type << "(" << *this->_alive << ", "
             << this->_block.get() << ")";
    }

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Egg::Type const type)
    {
      switch (type)
        {
        case Egg::Type::transient:
          {
            stream << "transient";
            break;
          }
        case Egg::Type::permanent:
          {
            stream << "permanent";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown egg type: '%s'",
                                          static_cast<int>(type)));
          }
        }

      return (stream);
    }
  }
}
