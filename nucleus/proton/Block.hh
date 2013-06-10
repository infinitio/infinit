#ifndef NUCLEUS_PROTON_BLOCK_HH
# define NUCLEUS_PROTON_BLOCK_HH

# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>
# include <elle/io/Dumpable.hh>
# include <elle/concept/Uniquable.hh>
# include <elle/utility/Time.hh>
# include <elle/serialize/construct.hh>
# include <elle/serialize/Serializable.hh>

# include <cryptography/fwd.hh>
# include <cryptography/Digest.hh>
# include <cryptography/oneway.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Network.hh>
# include <nucleus/proton/Family.hh>
# include <nucleus/proton/State.hh>
# include <nucleus/neutron/Component.hh>

# include <boost/noncopyable.hpp>

namespace nucleus
{
  namespace proton
  {
    /// This class abstracts the notion of storable block of data.
    ///
    /// Note that every block is identified by an address which can be
    /// computed through the bind() method.
    ///
    /// The state attribute indicates whether the block has
    /// been modified; therefore, this indicator is never serialized.
    ///
    /// A block contains some information which indicate the nature
    /// of the block such as _network, _family and _component. In addition,
    /// the block embeds the hash of the block creator so as to be able
    /// to authenticate later, for deleting the block for instance. Finally,
    /// a creation timestamp and a salt are generated at creation so as
    /// to distinguish blocks with the same content. For more information on
    /// the creation timestamp and salt, please refer to the specific physical
    /// blocks, especially to the bind() method which makes use of these to
    /// compute the block address.
    class Block:
      public elle::io::Dumpable,
      public elle::Printable,
      public elle::serialize::Serializable<>,
      public elle::concept::Uniquable<>,
      private boost::noncopyable
    {
      /*----------.
      | Constants |
      `----------*/
    public:
      struct Constants
      {
        static cryptography::oneway::Algorithm const oneway_algorithm;
      };

      /*-------------.
      | Construction |
      `-------------*/
    public:
      Block(); // XXX[to deserialize]
      Block(Network const network,
            Family const family,
            neutron::Component const component,
            cryptography::PublicKey const& creator_K);
      ELLE_SERIALIZE_CONSTRUCT_DECLARE(Block);
      virtual
      ~Block();

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Computes the address of the block.
      virtual
      Address
      bind() const = 0;
      /// Validates the block's content according to its address.
      virtual
      void
      validate(Address const& address) const = 0;

      /*----------.
      | Operators |
      `----------*/
    public:
      ELLE_OPERATOR_NO_ASSIGNMENT(Block);

      /*-----------.
      | Interfaces |
      `-----------*/
    public:
      // serialize
      ELLE_SERIALIZE_FRIEND_FOR(Block);
      // dumpable
      elle::Status
      Dump(const elle::Natural32 = 0) const;
      // printable
      virtual
      void
      print(std::ostream& stream) const;

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      /// Identifies the network in which lies the block.
      ELLE_ATTRIBUTE_R(Network, network);
      /// Identifies the physical nature of the block i.e its
      /// construct so as to ensure its integrity.
      ELLE_ATTRIBUTE_R(Family, family);
      /// Indicates the logical nature of the block i.e the high-level
      /// information.
      ELLE_ATTRIBUTE_R(neutron::Component, component);
      /// The creator attribute is used for authenticating certain
      /// actions such as deleting a block. Given such a request,
      /// a node holding a replica of the block would have to make
      /// sure the requesting user is the block creator. For that
      /// purpose information is kept in the block so as to authenticate
      /// this creator.
      ///
      /// Note however, that just enough information is kept. In this
      /// case, the creator's public key is not kept. Instead, only a
      /// hash is serialized since enough to proceed to an authentication.
      ELLE_ATTRIBUTE(cryptography::Digest, creator);
      /// The block creation timestamp. This timestamp is especially
      /// useful to distinguish two block created by the same creator.
      ELLE_ATTRIBUTE_R(elle::utility::Time, creation_timestamp);
      /// A random salt so as to further differentiate two blocks being
      /// created by the same user at the exact same time.
      ELLE_ATTRIBUTE_R(elle::Natural64, salt);
      /// Indicates the state of the block such as clean or dirty for
      /// example. This attribute is never serialized.
      ELLE_ATTRIBUTE_RW(State, state);
    };
  }
}

# include <nucleus/proton/Block.hxx>

#endif
