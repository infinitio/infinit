#include <etoile/depot/Depot.hh>

#include <lune/Descriptor.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Block.hh>
#include <nucleus/proton/Revision.hh>
#include <nucleus/proton/Contents.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Group.hh>

#include <hole/Hole.hh>

#include <elle/assert.hh>

#include <Infinit.hh>

namespace etoile
{
  namespace depot
  {
    /*-------------.
    | Construction |
    `-------------*/

    Depot::Depot(hole::Hole* hole,
                 nucleus::proton::Address const& root_address):
      _hole(hole),
      _root_address(root_address)
    {}

    nucleus::proton::Network const&
    Depot::network() const
    {
      return this->_hole->storage().network();
    }

    /*-----------.
    | Operations |
    `-----------*/

    elle::Status
    Depot::Origin(nucleus::proton::Address& address)
    {
      address = this->_root_address;
      return elle::Status::Ok;
    }

    elle::Status
    Depot::Push(const nucleus::proton::Address& address,
                const nucleus::proton::Block& block)
    {
      this->_hole->push(address, block);
      return elle::Status::Ok;
    }

    std::unique_ptr<nucleus::neutron::Object>
    Depot::pull_object(nucleus::proton::Address const& address,
                       nucleus::proton::Revision const & revision)
    {
      return (Depot::pull<nucleus::neutron::Object>(address, revision));
    }

    std::unique_ptr<nucleus::neutron::Group>
    Depot::pull_group(nucleus::proton::Address const& address,
                      nucleus::proton::Revision const& revision)
    {
      return (Depot::pull<nucleus::neutron::Group>(address, revision));
    }

    std::unique_ptr<nucleus::proton::Contents>
    Depot::pull_contents(nucleus::proton::Address const& address)
    {
      return (Depot::pull<nucleus::proton::Contents>(address));
    }

    ///
    /// this method permanently removes a block from the storage layer.
    ///
    elle::Status        Depot::Wipe(const nucleus::proton::Address& address)
    {
      // call the Hole.
      this->_hole->wipe(address);

      return elle::Status::Ok;
    }
  }
}
