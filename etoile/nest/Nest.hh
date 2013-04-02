#ifndef ETOILE_NEST_NEST_HH
# define ETOILE_NEST_NEST_HH

# include <elle/types.hh>

# include <etoile/nest/Pod.hh>
# include <etoile/gear/fwd.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Nest.hh>

# include <cryptography/fwd.hh>

# include <boost/noncopyable.hpp>

# include <map>

namespace etoile
{
  namespace nest
  {
    /// Provide a nest implementation which keeps content blocks in main memory.
    ///
    /// However, should a threshold be reached, the nest would pick the least
    /// recently used blocks and pre-published them onto the storage layer.
    class Nest:
      public nucleus::proton::Nest,
      private boost::noncopyable
    {
      /*------.
      | Types |
      `------*/
    public:
      typedef std::map<nucleus::proton::Egg*, Pod*> Pods;
      typedef std::map<nucleus::proton::Address const, Pod*> Addresses;
      typedef std::list<Pod*> History;

      /*-------------.
      | Construction |
      `-------------*/
    public:
      /// Construct a nest by providing the length of the secret key with which
      /// the modified blocks will be encrypted.
      Nest(elle::Natural32 const secret_length,
           nucleus::proton::Limits const& limits,
           nucleus::proton::Network const& network,
           cryptography::PublicKey const& agent_K);
      virtual
      ~Nest();

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Transcribe the nest's state into a transcript representing the
      /// operations to perform on the storage layer: store a block, remove
      /// another one etc.
      gear::Transcript
      transcribe();
    private:
      /// Return true if the given address' block lies in the nest.
      elle::Boolean
      _exist(nucleus::proton::Address const& address) const;
      /// Insert the pod in the pods container.
      void
      _insert(Pod* pod);
      /// Create a mapping between an address and an egg.
      void
      _map(nucleus::proton::Address const& address,
           Pod* pod);
      /// Return the pod associated with the given egg.
      Pod*
      _lookup(std::shared_ptr<nucleus::proton::Egg> const& egg) const;
      /// Return the pod associated with the given address.
      Pod*
      _lookup(nucleus::proton::Address const& address) const;
      /// Remove the mapping for the given address.
      void
      _unmap(nucleus::proton::Address const& address);
      /// Try to optimize the nest according to internal limits and conditions.
      void
      _optimize();
      /// Load the block from the depot and set it in the pod's egg.
      void
      _load(Pod* pod);
      /// Add the given pod at the end of the history queue for easing the
      /// pre-publication process.
      void
      _queue(Pod* pod);
      /// Remove the given pod from the history, probably because its block
      /// is being used.
      void
      _unqueue(Pod* pod);

      /*-----------.
      | Interfaces |
      `-----------*/
    public:
      // dumpable
      elle::Status
      Dump(const elle::Natural32 = 0) const;
      // nest
      nucleus::proton::Handle
      attach(nucleus::proton::Contents* block);
      void
      detach(nucleus::proton::Handle& handle);
      void
      load(nucleus::proton::Handle& handle);
      void
      unload(nucleus::proton::Handle& handle);

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      /// The set of pods tracking the various content blocks.
      ///
      /// Note that this container is index by the address of the egg tracked
      /// by the pod so as to retrieve a pod based on an egg.
      ELLE_ATTRIBUTE(Pods, pods);
      /// Contain the addresses of the permanents blocks for which
      /// an egg exist in the nest.
      ELLE_ATTRIBUTE(Addresses, addresses);
      /// The LRU-sorted history of pods.
      ELLE_ATTRIBUTE(History, history);
      /// The length of the secret key with which the blocks having been
      /// created or modified will be encrypted.
      ELLE_ATTRIBUTE(elle::Natural32, secret_length);
    };
  }
}

#endif
