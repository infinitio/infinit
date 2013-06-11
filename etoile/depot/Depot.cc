#include <etoile/depot/Depot.hh>

#include <infinit/Descriptor.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Block.hh>
#include <nucleus/proton/Revision.hh>
#include <nucleus/proton/Contents.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Group.hh>

#include <hole/Hole.hh>

#include <elle/assert.hh>
#include <elle/serialize/extract.hh>

#include <common/common.hh>

#include <Infinit.hh>

namespace etoile
{
  namespace depot
  {
    /*-----------------------------.
    | Global Hole instance (FIXME) |
    `-----------------------------*/
    static
    hole::Hole*&
    _hole()
    {
      static hole::Hole* hole(nullptr);
      return hole;
    }

    hole::Hole&
    hole()
    {
      ELLE_ASSERT(_hole());
      return *_hole();
    }

    void
    hole(hole::Hole* hole)
    {
      ELLE_ASSERT(!_hole() || !hole);
      _hole() = hole;
    }

    elle::Boolean
    have_hole()
    {
      return (_hole() != nullptr);
    }

//
// ---------- methods ---------------------------------------------------------
//

    /// this method returns the address of the network's root block.
    elle::Status        Depot::Origin(nucleus::proton::Address& address)
    {
      // FIXME: do not re-parse the descriptor every time.
      Descriptor descriptor(
        elle::serialize::from_file(
          common::infinit::descriptor_path(Infinit::User, Infinit::Network)));

      address = descriptor.meta().root_address();

      return elle::Status::Ok;
    }

    ///
    /// this method stores the given block in the underlying storage layer.
    ///
    elle::Status        Depot::Push(const nucleus::proton::Address& address,
                                    const nucleus::proton::Block& block)
    {
      hole().push(address, block);
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
      hole().wipe(address);

      return elle::Status::Ok;
    }

  }
}
