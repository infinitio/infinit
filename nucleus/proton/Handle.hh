#ifndef NUCLEUS_PROTON_HANDLE_HH
# define NUCLEUS_PROTON_HANDLE_HH

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>
# include <elle/serialize/construct.hh>

# include <cryptography/fwd.hh>
# include <cryptography/SecretKey.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Address.hh>

namespace nucleus
{
  namespace proton
  {
    /// XXX
    class Handle:
      public elle::Printable
    {
      /*-------------.
      | Enumerations |
      `-------------*/
    public:
      /// Define whether the block is located in the nest or not, in which
      /// case only the address/secret tuple would be kept.
      enum class Phase
        {
          unnested,
          nested
        };

      /*-------------.
      | Construction |
      `-------------*/
    public:
      Handle(); // XXX[to deserialize]
      /// Construct a handle for a permanent block i.e by providing its
      /// address and the secret for decrypting it.
      Handle(Address const& address,
             cryptography::SecretKey const& secret);
      /// Construct a handle for a transient block i.e a block which has
      /// not yet been published in the storage layer.
      Handle(std::shared_ptr<Egg>& egg);
      /// Copy constructor.
      Handle(Handle const& other);
      /// Destructor.
      virtual
      ~Handle();

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Return the address associated with the handle.
      Address const&
      address() const;
      /// Return the secret associated with the handle.
      cryptography::SecretKey const&
      secret() const;
      /// Return the egg.
      std::shared_ptr<Egg> const&
      egg() const;
      /// Return the egg.
      std::shared_ptr<Egg>&
      egg();
      /// Indicate the handle that a place has been found in the nest
      /// for the block associated with this handle.
      ///
      /// The given egg represents this place and can be used to directly
      /// access the block.
      void
      place(std::shared_ptr<Egg>& egg);
      /// Instruct the handle to evolve to an egg by allocating one based
      /// on the current handle's clef.
      ///
      /// This egg is the base of all the future ones which will also reference
      /// it.
      void
      evolve();
      /// Reset the handle with the given address/secret tuple, following
      /// a sealing process for instance.
      void
      reset(Address const& address,
            cryptography::SecretKey const& secret);
      /// Return the state of the block referenced by the handle.
      ///
      /// This allows someone to know the state of a block without necessarily
      /// loading it.
      State
      state() const;
      /// Update the state of the block referenced by the handle.
      void
      state(State const state);

      /*----------.
      | Operators |
      `----------*/
    public:
      elle::Boolean
      operator ==(Handle const&) const;
      ELLE_OPERATOR_NEQ(Handle);
      ELLE_OPERATOR_NO_ASSIGNMENT(Handle);

      /*-----------.
      | Interfaces |
      `-----------*/
    public:
      // dumpable
      elle::Status
      Dump(const elle::Natural32 = 0) const;
      // printable
      virtual
      void
      print(std::ostream& stream) const;
      // serializable
      ELLE_SERIALIZE_FRIEND_FOR(Handle);

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      ELLE_ATTRIBUTE_R(Phase, phase);
      union
      {
        /// Contain the address and secret of the block referenced
        /// by the handle.
        ///
        /// As soon as the handle is requested to be loaded, the
        /// clef is passed to the egg which represents the block's nested state.
        Clef* _clef;
        /// Reference the block descriptor directly within the
        /// nest.
        ///
        /// This descriptor is shared by all the handles referencing
        /// the same block.
        ///
        /// This way, the block can be directly accessed as long as
        /// it remains in main memory. Otherwise, should the egg not
        /// be present, it can be loaded from the nest.
        std::shared_ptr<Egg>* _egg;
      };
    };

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Handle::Phase const phase);
  }
}

# include <nucleus/proton/Handle.hxx>

#endif
