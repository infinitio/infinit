#include <nucleus/proton/Node.hh>
#include <nucleus/proton/Nest.hh>
#include <nucleus/proton/Seam.hh>
#include <nucleus/proton/Quill.hh>
#include <nucleus/proton/Value.hh>
#include <nucleus/neutron/Catalog.hh>
#include <nucleus/neutron/Data.hh>
#include <nucleus/neutron/Reference.hh>

#include <elle/log.hh>

namespace nucleus
{
  namespace proton
  {
    /*-------------.
    | Construction |
    `-------------*/

    Node::Node():
      _nest(nullptr),
      _footprint(0),
      _state(State::clean)
    {
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(Node)
    {
      this->_nest = nullptr;
      this->_footprint = 0;
      this->_state = State::clean;
    }

    Node::~Node()
    {
    }

//
// ---------- methods ---------------------------------------------------------
//

    void
    Node::nest(Nest& nest)
    {
      this->_nest = &nest;
    }

    Nest&
    Node::nest()
    {
      assert(this->_nest != nullptr);

      return (*this->_nest);
    }

    Footprint
    Node::footprint() const
    {
      return (this->_footprint);
    }

    void
    Node::footprint(Footprint const footprint)
    {
      this->_footprint = footprint;
    }

    State
    Node::state() const
    {
      return (this->_state);
    }

    void
    Node::state(State const state)
    {
      this->_state = state;
    }

//
// ---------- dumpable --------------------------------------------------------
//

    elle::Status
    Node::Dump(const elle::Natural32 margin) const
    {
      elle::String alignment(margin, ' ');

      std::cout << alignment << "[Node]" << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Footprint] " << std::dec << this->_footprint
                << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << "[State] " << std::dec << this->_state
                << std::endl;

      return elle::Status::Ok;
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Node::print(std::ostream& stream) const
    {
      stream << "node";
    }
  }
}
