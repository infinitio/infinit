#ifndef NUCLEUS_PROTON_EGG_HH
# define NUCLEUS_PROTON_EGG_HH

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>

# include <cryptography/fwd.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <nucleus/proton/fwd.hh>

# include <boost/noncopyable.hpp>

namespace nucleus
{
  namespace proton
  {
    /// Represent a block in the nest.
    ///
    /// An egg can be created two ways, either by providing the address
    /// of the block along with the secret key for decrypting it or by
    /// providing the block to track along with a temporary address and
    /// secret, since such elements are unknown for transient blocks.
    ///
    /// It is very important to understand that an egg can be referenced
    /// by multiple handles.
    class Egg:
      public elle::Printable,
      private boost::noncopyable
    {
      /*-------------.
      | Enumerations |
      `-------------*/
    public:
      /// Define the type of the block referenced by the egg. A transient
      /// block is a block which has been created and attached to the nest
      /// but which has not been encrypted and bound yet; hence does not have
      /// a secret key and address. Such a block can be destroyed in which
      /// case nothing really happens or published onto the storage layer. On
      /// the opposite, a permanent block is a block which already lives in
      /// the storage layer, which can be accessed through its address and for
      /// which one needs a secret to be able to decrypt its content.
      enum class Type
        {
          transient,
          permanent
        };

      /*-------------.
      | Construction |
      `-------------*/
    public:
      /// Constructor for permanent blocks i.e living in the storage layer.
      Egg(Address const& address,
          cryptography::SecretKey const& secret);
      /// Constructor for permanent blocks through the block's clef which
      /// is handed over, transferring the ownership to the egg.
      Egg(Clef const* clef);
      /// Constructor for transient blocks: the block is provided along with
      /// a temporary (and probably invalid) address and secret. The idea behind
      /// these temporary elements is to enable the system to compute their
      /// footprint and end up with a correct number. Thus, the length of the
      /// secret should comply with the one used for sealing the block later on.
      ///
      /// Note that the given block's ownership is transferred.
      Egg(Contents* block,
          Address const& address,
          cryptography::SecretKey const& secret);

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Return true if the tracked block has history i.e a previous version
      /// of the block exists in the storage layer.
      elle::Boolean
      has_history() const;
      /// Return the address associated with the tracked block.
      Address const&
      address() const;
      /// Return the secret key associated with the tracked block though, as for
      /// address(), the returned value may not be final i.e temporary.
      cryptography::SecretKey const&
      secret() const;
      /// Return the clef (i.e address/secret) referencing the alive version of
      /// the tracked block.
      Clef const&
      alive() const;
      /// Return the clef of the historical version (in the storage layer) of
      /// the tracked block.
      Clef const&
      historical() const;
      /// Reset the address/secret tuple associated with the tracked block,
      /// following a sealing process for instance.
      void
      reset(Address const& address,
            cryptography::SecretKey const& secret);

      /*----------.
      | Operators |
      `----------*/
    public:
      elle::Boolean
      operator ==(Egg const&) const;
      ELLE_OPERATOR_NEQ(Egg);
      ELLE_OPERATOR_NO_ASSIGNMENT(Egg);

      /*-----------.
      | Interfaces |
      `-----------*/
    public:
      // printable
      virtual
      void
      print(std::ostream& stream) const;

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      ELLE_ATTRIBUTE_R(Type, type);
      /// The clef for accessing the block currently lying in main
      /// memory. In a way, this clef could be considered as referencing
      /// a temporary block since the block has not yet been published
      /// onto the storage layer.
      ELLE_ATTRIBUTE(std::unique_ptr<Clef const>, alive);
      /// The clef referencing the historical --- i.e existing/previous ---
      /// version of the block which lies on the storage layer.
      ELLE_ATTRIBUTE(std::unique_ptr<Clef const>, historical);
      /// The actual block though the pointer may be null should
      /// have the block been moved somewhere else, on the disk
      /// for example, so as to reduce the main memory consumption.
      ELLE_ATTRIBUTE_RX(std::unique_ptr<Contents>, block);
    };

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Egg::Type const type);
  }
}

#endif
