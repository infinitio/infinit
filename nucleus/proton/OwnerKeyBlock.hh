#ifndef NUCLEUS_PROTON_OWNERKEYBLOCK_HH
# define NUCLEUS_PROTON_OWNERKEYBLOCK_HH

# include <elle/types.hh>
# include <elle/utility/Time.hh>

# include <cryptography/PublicKey.hh>
# include <cryptography/Signature.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/MutableBlock.hh>
# include <nucleus/neutron/Component.hh>
# include <nucleus/neutron/Subject.hh>

namespace nucleus
{
  namespace proton
  {

    /// This class represents a mutable block which is statically linked
    /// to a specific user i.e the block owner.
    ///
    /// This construct is interesting because, given multiple such blocks,
    /// the owner only has to remember her personal key pair in order to
    /// update them rather than keeping the private key associated with
    /// with every mutable block as for PublicKeyBlocks.
    ///
    /// The idea is to generate a key pair (as for PublicKeyBlocks). The
    /// generated public key will act as the block's public key so that
    /// the block address will be computed by applying a one-way function
    /// on it. Then, the generated private key (i.e block private key) is
    /// used to sign the owner's public key. This way, a static link is
    /// created between the block address, the block's public key and the
    /// owner's public key (since the signature can be verified with the
    /// block's public key). Finally, the owner will be able to sign
    /// the block's content with its private key. Thus, the user can
    /// create as many blocks as wanted with a single key pair.
    class OwnerKeyBlock:
      public MutableBlock
    {
      /*----------.
      | Constants |
      `----------*/
    public:
      struct Constants
      {
        static elle::Natural32 const keypair_length;
      };

      //
      // construction
      //
    public:
      OwnerKeyBlock(); // XXX[to deserialize]
      OwnerKeyBlock(Network const& network,
                    neutron::Component component,
                    cryptography::PublicKey const& creator_K);
      ~OwnerKeyBlock();
    private:
      /// Initializes the instance based on the key pair generated for
      /// the block.
      OwnerKeyBlock(Network const& network,
                    neutron::Component component,
                    cryptography::PublicKey const& creator_K,
                    cryptography::KeyPair const& block_keypair);

      //
      // methods
      //
    public:
      /// The subject of the block owner.
      neutron::Subject const&
      owner_subject();

      //
      // interfaces
      //
    public:
      // block
      virtual
      Address
      bind() const;
      virtual
      void
      validate(Address const& address) const;
      // dumpable
      elle::Status
      Dump(const elle::Natural32 = 0) const;
      // printable
      virtual
      void
      print(std::ostream& stream) const;
      // serializable
      ELLE_SERIALIZE_FRIEND_FOR(OwnerKeyBlock);

      //
      // attributes
      //
    private:
      ELLE_ATTRIBUTE(cryptography::PublicKey, block_K);
      ELLE_ATTRIBUTE_R(cryptography::PublicKey, owner_K);
      ELLE_ATTRIBUTE(cryptography::Signature, owner_signature);
      ELLE_ATTRIBUTE(neutron::Subject*, owner_subject);
    };

  }
}

# include <nucleus/proton/OwnerKeyBlock.hxx>

#endif
