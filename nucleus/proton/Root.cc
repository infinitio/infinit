#include <nucleus/proton/Root.hh>

namespace nucleus
{
  namespace proton
  {
    /*-------------.
    | Construction |
    `-------------*/

    Root::Root()
    {
    }

    Root::Root(Address const& address,
               Height const& height,
               Capacity const& capacity):
      _address(address),
      _height(height),
      _capacity(capacity)
    {
    }

    Root::Root(Root const& other):
      _address(other._address),
      _height(other._height),
      _capacity(other._capacity)
    {
    }

    Root::Root(Root&& other):
      _address(std::move(other._address)),
      _height(std::move(other._height)),
      _capacity(std::move(other._capacity))
    {
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Root::print(std::ostream& stream) const
    {
      stream << "("
             << this->_address
             << ", "
             << this->_height
             << ", "
             << this->_capacity
             << ")";
    }
  }
}
