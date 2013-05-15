#ifndef LUNE_DESCRIPTOR_HH
# define LUNE_DESCRIPTOR_HH

# include <elle/Version.hh>
# include <elle/Printable.hh>
# include <elle/serialize/fwd.hh>
# include <elle/serialize/Format.hh>
# include <elle/serialize/construct.hh>

# include <cryptography/Signature.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <lune/fwd.hh>

# include <hole/Authority.hh>
# include <hole/Model.hh>
# include <hole/fwd.hh>

# include <nucleus/proton/Address.hh>
# include <nucleus/neutron/fwd.hh>
# include <nucleus/neutron/Group.hh>
# include <nucleus/neutron/Subject.hh>

# include <horizon/Policy.hh>

//
// ---------- Descriptor ------------------------------------------------------
//

namespace lune
{
  /*---------------------.
  | Forward Declarations |
  `---------------------*/
  namespace descriptor
  {
    struct Meta;
    struct Data;
  }

  /// Represent a network descriptor which, as the name indicates, describes
  /// a network's parameters such as the administrator's public key, the
  /// network unique identifier etc. along with configuration values which
  /// could change over time should the administrator decide so. These
  /// configuration values include a network name, an openness which defines
  /// how open the network is to other users and the policy which defines the
  /// sharing behaviour i.e are files/directories/etc. shared in read-only with
  /// everybody by default, in read-write or kept private to the object owner.
  /// Finally the configuration embeds the reference version of the Infinit
  /// software and the reference formats of the blocks. These are provided in
  /// order to overcome/ the issue described below.
  ///
  /// One can quite easily create a descriptor from both a meta section, which
  /// is sealed by an authority, and a data section which is signed by the
  /// network administrator, hence can be modified over time.
  ///
  /// The following details the process of creating such a descriptor.
  ///
  /// First, the meta elements are signed with the authority, generating a
  /// signature which is then used, along with the other elements, for creating
  /// the meta section:
  ///
  ///   cryptography::Signature meta_signature =
  ///     authority.sign(
  ///       meta::hash(id, admin_K, model, root, everybody, history, extent));
  ///   Meta meta_section(id, admin_K, model, root, everybody, history, extent,
  ///                     meta_signature);
  ///
  /// The same is done with the data section except that it is signed with the
  /// administrator's private key:
  ///
  ///   cryptography::Signature data_signature =
  ///     admin_k.sign(
  ///       data::hash(name, openness, policy, version, format_...);
  ///   Data data_section(name, openness, policy, version, format_...,
  ///                     data_signature);
  ///
  /// Finally, a descriptor is instantiated from both the sections.
  ///
  ///   Descriptor descriptor(meta_section, data_section);
  class Descriptor:
    public elle::Printable,
    public elle::concept::MakeFileable<Descriptor>,
    public elle::concept::MakeUniquable<Descriptor>
  {
    /*-------------.
    | Construction |
    `-------------*/
  public:
    /// XXX[should we keep this?]
    Descriptor(elle::String const& user,
               elle::String const& network);
    /// Construct a descriptor from both its meta and data sections.
    Descriptor(descriptor::Meta const& meta,
               descriptor::Data const& data);
    /// Construct a descriptor from both its meta and data sections whose
    /// ownership is transferred to the descriptor.
    Descriptor(descriptor::Meta&& meta,
               descriptor::Data&& data);
    Descriptor(Descriptor const& other);
    Descriptor(Descriptor&& other) = default;
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Descriptor);

    /*--------.
    | Methods |
    `--------*/
  public:
    /// Return true if the whole descriptor is valid, including both the
    /// meta and data sections.
    elle::Boolean
    validate(elle::Authority const& authority) const;
    /// Return the meta section.
    descriptor::Meta const&
    meta() const;
    /// Return the data secton.
    descriptor::Data const&
    data() const;
    /// Update the data section.
    void
    data(descriptor::Data const& data);
    /// Update the data section by transferring the ownership of the given one.
    void
    data(descriptor::Data&& data);

    /*----------.
    | Operators |
    `----------*/
  public:
    ELLE_OPERATOR_NO_ASSIGNMENT(Descriptor);

    /*-----------.
    | Interfaces |
    `-----------*/
  public:
    // serializable
    ELLE_SERIALIZE_FRIEND_FOR(Descriptor);
    // fileable
    ELLE_CONCEPT_FILEABLE_METHODS();
    void
    load(elle::String const& user,
         elle::String const& network);
    void
    store(Identity const& identity) const;
    static
    void
    erase(elle::String const& user,
          elle::String const& network);
    static
    elle::Boolean
    exists(elle::String const& user,
           elle::String const& network);
    // printable
    virtual
    void
    print(std::ostream& stream) const;

    /*-----------.
    | Attributes |
    `-----------*/
  private:
    ELLE_ATTRIBUTE(std::unique_ptr<descriptor::Meta>, meta);
    ELLE_ATTRIBUTE(std::unique_ptr<descriptor::Data>, data);
  };
}

//
// ---------- Meta ------------------------------------------------------------
//

namespace lune
{
  namespace descriptor
  {
    /// Represent the descriptor elements which cannot change over time because
    /// sealed by the authority.
    class Meta:
      public elle::Printable
    {
      /*--------.
      | Friends |
      `--------*/
    public:
      // Required for the format 0.
      friend class Descriptor;

      /*-------------.
      | Construction |
      `-------------*/
    public:
      Meta(); // XXX[deserialization instead]
      /// Construct a meta section based on the given elements.
      Meta(elle::String const& identifier,
           cryptography::PublicKey const& administrator_K,
           hole::Model const& model,
           nucleus::proton::Address const& root,
           nucleus::neutron::Group::Identity const& everybody,
           elle::Boolean history,
           elle::Natural32 extent,
           cryptography::Signature const& signature);
      /// Construct a meta section based on the given elements whose
      /// ownership is transferred to the descriptor.
      Meta(elle::String&& identifier,
           cryptography::PublicKey&& administrator_K,
           hole::Model&& model,
           nucleus::proton::Address&& root,
           nucleus::neutron::Group::Identity&& everybody,
           elle::Boolean history,
           elle::Natural32 extent,
           cryptography::Signature&& signature);
      Meta(Meta const& other);
      Meta(Meta&& other) = default;
      ELLE_SERIALIZE_CONSTRUCT_DECLARE(Meta);

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Return trun if the descriptor is valid, false otherwise.
      elle::Boolean
      validate(elle::Authority const& authority) const;
      /// Return the subject representing the everybody's group.
      nucleus::neutron::Subject const&
      everybody_subject() const;

      /*----------.
      | Operators |
      `----------*/
    public:
      ELLE_OPERATOR_NO_ASSIGNMENT(Meta);

      /*-----------.
      | Interfaces |
      `-----------*/
    public:
      // serializable
      ELLE_SERIALIZE_FRIEND_FOR(Descriptor);
      ELLE_SERIALIZE_FRIEND_FOR(Meta);
      // printable
      virtual
      void
      print(std::ostream& stream) const;

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      ELLE_ATTRIBUTE_R(elle::String, identifier);
      ELLE_ATTRIBUTE_R(cryptography::PublicKey, administrator_K);
      ELLE_ATTRIBUTE_R(hole::Model, model);
      ELLE_ATTRIBUTE_R(nucleus::proton::Address, root);
      ELLE_ATTRIBUTE_R(nucleus::neutron::Group::Identity, everybody_identity);
      ELLE_ATTRIBUTE_P(std::unique_ptr<nucleus::neutron::Subject>,
                       everybody_subject,
                       mutable);
      ELLE_ATTRIBUTE_R(elle::Boolean, history);
      ELLE_ATTRIBUTE_R(elle::Natural32, extent);
      ELLE_ATTRIBUTE(cryptography::Signature, signature);
    };

    namespace meta
    {
      /*----------.
      | Functions |
      `----------*/

      /// Return a digest of the most fondamental elements composing the meta
      /// section.
      cryptography::Digest
      hash(elle::String const& identifier,
           cryptography::PublicKey const& administrator_K,
           hole::Model const& model,
           nucleus::proton::Address const& root,
           nucleus::neutron::Group::Identity const& everybody,
           elle::Boolean history,
           elle::Natural32 extent);
    }
  }
}

//
// ---------- Data ------------------------------------------------------------
//

namespace lune
{
  namespace descriptor
  {
    /// Represent the descriptor elements which are controlled by the network
    /// administrator. This means that this section can be modified (i.e
    /// recreated) should the administrator want to change the network behavior.
    ///
    /// Let us recall that every block when manipulated, lives in main memory
    /// but when transmitted over the network or stored is serialized (i.e in an
    /// architecture-independent format) so as to be rebuilt by any other
    /// computer no matter its platform, operating system etc. Besides, since
    /// the Infinit software evolves, a block, say the ContentHashBlock for
    /// explanatory puroposes, could embed an additional attribute, for example
    /// a timestamp. Therefore, depending on the version of the Infinit
    /// software, a block could be represented (in main memory) and serialized
    /// in two ways. More precisely, the way a block is serialized and
    /// deserialized (i.e rebuilt in main memory) depends on the serializer, in
    /// this case the ContentHashBlock serializer.
    ///
    /// Let us imagine a scenario involving two nodes. The first, X, runs
    /// Infinit in its version 8 while the second, Y, runs Infinit in a newer
    /// version, say 10. X decides to update a block B and therefore publish it
    /// to the network to replicate it. Since X runs Infinit in its version 8,
    /// the block B has been serialized in the latest ContentHashBlock's format
    /// supported by Infinit version 8. Let us say this version of Infinit has a
    /// ContentHashBlock serializer which produces blocks in the format 2.
    /// However, Infinit version 10 embeds an additional attribute so that such
    /// blocks are now serialized in format 3. Therefore, when the block B,
    /// serialized in format 2, is received, the node Y must be able to
    /// deserialize and manipulate it though it has been serialized in an older
    /// format. In this case, Y will load the block B and actually manipulate a
    /// block in the format 2 though the node Y's Infinit version can support
    /// the format 3. This means the additional attribute is not set and just
    /// ignored.
    ///
    /// Note that one could think that node Y could upgrade the block B to the
    /// newest version. Unfortunately this is not always possible because,
    /// often, after having been deserialized, a block is validated. If one
    /// upgrades the block, the validation will fail because of a corrupt
    /// signature (generated in format 2) failing to match the current content
    /// (recently upgraded to format 3 with an additional field which did not
    /// exist in format 2). Consequently, the operation consisting in upgrading
    /// a block from a format to the next should be performed at a very specific
    /// moment, depending on the nature of the block. For a ContentHashBlock
    /// (such as B) for instance, since such a block's address is function of
    /// its content, upgrading the block would modify its content and therefore
    /// its address. Thus, any other block referencing B would have to be
    /// updated as well. This is too difficult since we do not know which block
    /// references it. Besides, for some blocks, especially the ones which embed
    /// signatures, these signatures must be re-generated. Since only certain
    /// users can re-generate the signatures (often the block owner), the
    /// upgrading process can only be performed by the right user. As a example,
    /// for a ContentHashBlock, the system will perform the upgrading process
    /// if (i) the block has been modified so as to minimize the network
    /// communications, (ii) the user is the block owner (iii) and right before
    /// the call to bind() which computes the block address so as to take all
    /// the modifications (the upgrade) into account.
    ///
    /// In order to counter attacks from malicious nodes, the descriptor embeds
    /// the reference formats which are the ones supported by the version of
    /// Infinit also provided in the descriptor. As a rule of thumb, the network
    /// administrator uses its own Infinit version as a reference. Thus,
    /// whenever the administrator updates the Infinit software, its version
    /// along with the supported format are recorded in the descriptor which is
    /// upgraded and re-sealed. These reference formats are used whenever a node
    /// receives a block in an unkown format e.g a format higher than the one
    /// supported by the user's Infinit version. In this case, either the
    /// version of Infinit is too old to understand this format or the block is
    /// invalid: embedding a format 12345 for example though this format is
    /// supported by no Infinit version. Should a node receive such a block, it
    /// would check the format against the reference format of the given block
    /// type e.g ContentHashBlock. If the format is higher that the reference,
    /// the node would discard the block as invalid and would continue.
    /// Otherwise, a warning would be displayed asking the user to update the
    /// Infinit software in order to be able to understand the latest formats.
    ///
    /// Noteworthy is that formats should evolve quite rarely. Thus, the
    /// impropriety of forcing the user to update the Infinit software should
    /// not be too important.
    struct Data:
      public elle::Printable
    {
      /*--------.
      | Friends |
      `--------*/
    public:
      // Required for the format 0.
      friend class Descriptor;

      /*-------------.
      | Construction |
      `-------------*/
    public:
      Data(); // XXX[deserialization instead]
      /// Construct a data section from the given elements.
      Data(elle::String const& name,
           hole::Openness const& openness,
           horizon::Policy const& policy,
           elle::Version const& version,
           elle::serialize::Format const& format_block,
           elle::serialize::Format const& format_content_hash_block,
           elle::serialize::Format const& format_contents,
           elle::serialize::Format const& format_immutable_block,
           elle::serialize::Format const& format_imprint_block,
           elle::serialize::Format const& format_mutable_block,
           elle::serialize::Format const& format_owner_key_block,
           elle::serialize::Format const& format_public_key_block,
           elle::serialize::Format const& format_access,
           elle::serialize::Format const& format_attributes,
           elle::serialize::Format const& format_catalog,
           elle::serialize::Format const& format_data,
           elle::serialize::Format const& format_ensemble,
           elle::serialize::Format const& format_group,
           elle::serialize::Format const& format_object,
           elle::serialize::Format const& format_reference,
           elle::serialize::Format const& format_user,
           elle::serialize::Format const& format_identity,
           elle::serialize::Format const& format_descriptor,
           cryptography::Signature const& signature);
      /// Construct a data section by transferring the ownership of the
      /// given elements.
      Data(elle::String&& name,
           hole::Openness&& openness,
           horizon::Policy&& policy,
           elle::Version&& version,
           elle::serialize::Format&& format_block,
           elle::serialize::Format&& format_content_hash_block,
           elle::serialize::Format&& format_contents,
           elle::serialize::Format&& format_immutable_block,
           elle::serialize::Format&& format_imprint_block,
           elle::serialize::Format&& format_mutable_block,
           elle::serialize::Format&& format_owner_key_block,
           elle::serialize::Format&& format_public_key_block,
           elle::serialize::Format&& format_access,
           elle::serialize::Format&& format_attributes,
           elle::serialize::Format&& format_catalog,
           elle::serialize::Format&& format_data,
           elle::serialize::Format&& format_ensemble,
           elle::serialize::Format&& format_group,
           elle::serialize::Format&& format_object,
           elle::serialize::Format&& format_reference,
           elle::serialize::Format&& format_user,
           elle::serialize::Format&& format_identity,
           elle::serialize::Format&& format_descriptor,
           cryptography::Signature&& signature);
      Data(Data const& other) = default;
      Data(Data&& other) = default;
      ELLE_SERIALIZE_CONSTRUCT_DECLARE(Data);

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Return true if the data section is valid i.e has been signed with
      /// the administrator private key, false otherwise.
      elle::Boolean
      validate(cryptography::PublicKey const& administrator_K) const;

      /*----------.
      | Operators |
      `----------*/
    public:
      ELLE_OPERATOR_NO_ASSIGNMENT(Data);

      /*-----------.
      | Interfaces |
      `-----------*/
    public:
      // serializable
      ELLE_SERIALIZE_FRIEND_FOR(Descriptor);
      ELLE_SERIALIZE_FRIEND_FOR(Data);
      // printable
      virtual
      void
      print(std::ostream& stream) const;

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      ELLE_ATTRIBUTE_R(elle::String, name);
      ELLE_ATTRIBUTE_R(hole::Openness, openness);
      ELLE_ATTRIBUTE_R(horizon::Policy, policy);
      ELLE_ATTRIBUTE_R(elle::Version, version);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_block);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_content_hash_block);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_contents);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_immutable_block);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_imprint_block);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_mutable_block);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_owner_key_block);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_public_key_block);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_access);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_attributes);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_catalog);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_data);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_ensemble);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_group);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_object);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_reference);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_user);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_identity);
      ELLE_ATTRIBUTE_R(elle::serialize::Format, format_descriptor);
      ELLE_ATTRIBUTE_R(cryptography::Signature, signature);
    };

    namespace data
    {
      /*----------.
      | Functions |
      `----------*/

      /// Return a digest of the most fondamental elements composing a data
      /// section.
      cryptography::Digest
      hash(elle::String const& name,
           hole::Openness const& openness,
           horizon::Policy const& policy,
           elle::Version const& version,
           elle::serialize::Format const& format_block,
           elle::serialize::Format const& format_content_hash_block,
           elle::serialize::Format const& format_contents,
           elle::serialize::Format const& format_immutable_block,
           elle::serialize::Format const& format_imprint_block,
           elle::serialize::Format const& format_mutable_block,
           elle::serialize::Format const& format_owner_key_block,
           elle::serialize::Format const& format_public_key_block,
           elle::serialize::Format const& format_access,
           elle::serialize::Format const& format_attributes,
           elle::serialize::Format const& format_catalog,
           elle::serialize::Format const& format_data,
           elle::serialize::Format const& format_ensemble,
           elle::serialize::Format const& format_group,
           elle::serialize::Format const& format_object,
           elle::serialize::Format const& format_reference,
           elle::serialize::Format const& format_user,
           elle::serialize::Format const& format_identity,
           elle::serialize::Format const& format_descriptor);
    }
  }
}

# include <lune/Descriptor.hxx>

#endif
