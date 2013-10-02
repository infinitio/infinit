#ifndef ETOILE_DEPOT_DEPOT_HH
# define ETOILE_DEPOT_DEPOT_HH

# include <elle/types.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Revision.hh>
# include <nucleus/neutron/fwd.hh>

# include <etoile/gear/Transcript.hh>

# include <hole/fwd.hh>

# include <memory>
# include <set>

namespace etoile
{
  ///
  /// this namespace contains everything related to the storage layer
  /// abstraction especially through the communication with the Hole
  /// component.
  ///
  namespace depot
  {
    /// The storage layer.
    class Depot
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      Depot(hole::Hole* hole,
            nucleus::proton::Address const& root_address);
    private:
      ELLE_ATTRIBUTE(hole::Hole*, hole);
      ELLE_ATTRIBUTE_r(nucleus::proton::Network, network);
      ELLE_ATTRIBUTE(nucleus::proton::Address, root_address);
      ELLE_ATTRIBUTE_X(std::set<gear::Transcript*>, queue);

    /*-----------.
    | Operations |
    `-----------*/
    public:
      /// The address of the network's root block.
      elle::Status
      Origin(nucleus::proton::Address&);
      /// Store the given block in the underlying storage layer.
      elle::Status
      Push(const nucleus::proton::Address&,
           const nucleus::proton::Block&);
      /// XXX
      std::unique_ptr<nucleus::neutron::Object>
      pull_object(nucleus::proton::Address const& address,
                  nucleus::proton::Revision const & revision);
      /// XXX
      std::unique_ptr<nucleus::neutron::Group>
      pull_group(nucleus::proton::Address const& address,
                 nucleus::proton::Revision const& revision);
      /// XXX
      std::unique_ptr<nucleus::proton::Contents>
      pull_contents(nucleus::proton::Address const& address);
      /// XXX
      template <typename T>
      std::unique_ptr<T>
      pull(nucleus::proton::Address const& address,
           nucleus::proton::Revision const& revision =
             nucleus::proton::Revision::Last);
      elle::Status
      Wipe(const nucleus::proton::Address&);
    };
  }
}

# include <etoile/depot/Depot.hxx>

#endif
