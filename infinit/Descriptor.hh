#ifndef INFINIT_DESCRIPTOR_HH
# define INFINIT_DESCRIPTOR_HH

# include <elle/Version.hh>
# include <elle/Printable.hh>
# include <elle/serialize/fwd.hh>
# include <elle/serialize/Format.hh>
# include <elle/serialize/DynamicFormat.hh>
# include <elle/serialize/construct.hh>

# include <infinit/fwd.hh>

# include <cryptography/Signature.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <lune/fwd.hh>

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

namespace infinit
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
  ///   Meta meta(id, admin_K, model, root_address, ...,
  ///             authority.k());
  ///   Data data(name, openness, policy, version, format_...,
  ///             administrator_k());
  ///
  /// Finally, a descriptor is instantiated from both the sections which
  /// could be moved for example:
  ///
  ///   Descriptor descriptor(std::move(meta), std::move(data));
  ///
  /// Note that the network administrator can then modify the descriptor
  /// by providing an updated data section:
  ///
  ///   Data data(...);
  ///   descriptor.update(std::move(data));
  class Descriptor:
    public elle::Printable,
    public elle::serialize::DynamicFormat<Descriptor>
  {
    /*-------------.
    | Construction |
    `-------------*/
  public:
    /// Construct a descriptor from both its meta and data sections.
    explicit
    Descriptor(descriptor::Meta meta,
               descriptor::Data data);
    Descriptor(Descriptor&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Descriptor);
  private:
    Descriptor(Descriptor const&);

    /*--------.
    | Methods |
    `--------*/
  public:
    /// Return true if the whole descriptor is valid, including both the
    /// meta and data sections.
    ///
    /// Note that the _authority_ must provide a verify(signature, data)
    /// method.
    template <typename T>
    elle::Boolean
    validate(T const& authority) const;
    /// Return the meta section.
    descriptor::Meta const&
    meta() const;
    /// Return the data secton.
    descriptor::Data const&
    data() const;
    /// Update the data section.
    void
    update(descriptor::Data data);
    /// Update the data section by transferring the ownership of the given one.
    void
    update(std::unique_ptr<descriptor::Data>&& data);

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
    // printable
    virtual
    void
    print(std::ostream& stream) const;

    /*-----------.
    | Attributes |
    `-----------*/
  private:
    /// The meta section contains whatever information should not change
    /// over time.
    ELLE_ATTRIBUTE(std::unique_ptr<descriptor::Meta>, meta);
    /// Unlike the meta section, the elements in the data section can be
    /// modified by the network administrator.
    ELLE_ATTRIBUTE(std::unique_ptr<descriptor::Data>, data);
  };
}

//
// ---------- Meta ------------------------------------------------------------
//

namespace infinit
{
  namespace descriptor
  {
    /// Represent the descriptor elements which cannot change over time because
    /// sealed by the authority.
    class Meta:
      public elle::serialize::DynamicFormat<Meta>,
      public elle::Printable
    {
      /*--------.
      | Friends |
      `--------*/
    public:
      // Required for the format 0.
      friend class Descriptor;

      /*------.
      | Types |
      `------*/
    public:
      typedef elle::serialize::DynamicFormat<Meta> DynamicFormat;

      /*-------------.
      | Construction |
      `-------------*/
    public:
      Meta() {} // XXX[to remove when the new serialization will be fixed]
      /// Construct a meta section based on the given elements.
      Meta(elle::String identifier,
           cryptography::PublicKey administrator_K,
           hole::Model model,
           nucleus::proton::Address root_address,
           std::unique_ptr<nucleus::neutron::Object> root_object,
           nucleus::neutron::Group::Identity everybody_identity,
           elle::Boolean history,
           elle::Natural32 extent,
           cryptography::Signature signature);
      /// Construct a meta section based on the passed elements which will
      /// be signed with the given authority.
      ///
      /// Note that, through this helper, the _authority_ must provide a
      /// sign(data) method which returns a signature.
      template <typename T>
      Meta(elle::String identifier,
           cryptography::PublicKey administrator_K,
           hole::Model model,
           nucleus::proton::Address root_address,
           std::unique_ptr<nucleus::neutron::Object> root_object,
           nucleus::neutron::Group::Identity everybody_identity,
           elle::Boolean history,
           elle::Natural32 extent,
           T const& authority);
      Meta(Meta&& other);
      ELLE_SERIALIZE_CONSTRUCT_DECLARE(Meta);
    private:
      Meta(Meta const&);

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Return true if the descriptor is valid, false otherwise.
      ///
      /// Note that the _authority_ must provide a verify(signature, data)
      /// method which returns true if the given signature is valid.
      template <typename T>
      elle::Boolean
      validate(T const& authority) const;
      /// Return the subject representing the everybody's group.
      nucleus::neutron::Subject const&
      everybody_subject() const;
      /// Return the original (i.e at the network creation) content of the
      /// root directory object.
      nucleus::neutron::Object const&
      root_object() const;

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
      /// A unique identifier across all the networks created throughout
      /// the world.
      ELLE_ATTRIBUTE_R(elle::String, identifier);
      /// The public key of the network owner. Only the private key associated
      /// with it can re-sign the data section.
      ELLE_ATTRIBUTE_R(cryptography::PublicKey, administrator_K);
      /// The storage layer implementation.
      ELLE_ATTRIBUTE_R(hole::Model, model);
      /// The address of the root directory.
      ELLE_ATTRIBUTE_R(nucleus::proton::Address, root_address);
      /// The root directory in its initial form i.e empty.
      ELLE_ATTRIBUTE(std::unique_ptr<nucleus::neutron::Object>, root_object);
      /// This attribute is created should someone request it so as to ease
      /// the process of access control since the subject is the entity used
      /// to identify both users and groups.
      ELLE_ATTRIBUTE_R(nucleus::neutron::Group::Identity, everybody_identity);
      ELLE_ATTRIBUTE_P(std::unique_ptr<nucleus::neutron::Subject>,
                       everybody_subject,
                       mutable);
      /// Indicate whether or not this network supports history i.e versioning.
      ELLE_ATTRIBUTE_R(elle::Boolean, history);
      /// The maximum size of the blocks stored on the storage layer.
      ELLE_ATTRIBUTE_R(elle::Natural32, extent);
      /// A signature issued by the authority guaranteeing its validity.
      ELLE_ATTRIBUTE(cryptography::Signature, signature);
    };

    namespace meta
    {
      /*----------.
      | Functions |
      `----------*/

      /// Return a digest of the most fondamental elements composing the meta
      /// section.
      ///
      /// These are the elements which must be signed by the authority.
      cryptography::Digest
      hash(elle::String const& identifier,
           cryptography::PublicKey const& administrator_K,
           hole::Model const& model,
           nucleus::proton::Address const& root_address,
           std::unique_ptr<nucleus::neutron::Object> const& root_object,
           nucleus::neutron::Group::Identity const& everybody_identity,
           elle::Boolean history,
           elle::Natural32 extent);
      /// Compatibility with format 0.
      cryptography::Digest
      hash_0(elle::String const& identifier,
             cryptography::PublicKey const& administrator_K,
             hole::Model const& model,
             nucleus::proton::Address const& root_address,
             nucleus::neutron::Group::Identity const& everybody_identity,
             elle::Boolean history,
             elle::Natural32 extent);
    }
  }
}

//
// ---------- Data ------------------------------------------------------------
//

namespace infinit
{
  namespace descriptor
  {
    /// Represent the descriptor elements which are controlled by the network
    /// administrator. This means that this section can be modified (i.e
    /// recreated) should the administrator want to change the network
    /// behavior.
    ///
    /// Let us recall that every block when manipulated, lives in main memory
    /// but when transmitted over the network or stored is serialized (i.e in
    /// an architecture-independent format) so as to be rebuilt by any other
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
    /// supported by Infinit version 8. Let us say this version of Infinit has
    /// a ContentHashBlock serializer which produces blocks in the format 2.
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
    /// a block from a format to the next should be performed at a very
    /// specific moment, depending on the nature of the block. For a
    /// ContentHashBlock (such as B) for instance, since such a block's address
    /// is function of its content, upgrading the block would modify its
    /// content and therefore its address. Thus, any other block referencing B
    /// would have to be updated as well. This is too difficult since we do not
    /// know which block references it. Besides, for some blocks, especially
    /// the ones which embed signatures, these signatures must be re-generated.
    /// Since only certain users can re-generate the signatures (often the
    /// block owner), the upgrading process can only be performed by the right
    /// user. As a example, for a ContentHashBlock, the system will perform the
    /// upgrading process if (i) the block has been modified so as to minimize
    /// the network communications, (ii) the user is the block owner (iii) and
    /// right before the call to bind() which computes the block address so as
    /// to take all the modifications (the upgrade) into account.
    ///
    /// In order to counter attacks from malicious nodes, the descriptor embeds
    /// the reference formats which are the ones supported by the version of
    /// Infinit also provided in the descriptor. As a rule of thumb, the
    /// network administrator uses its own Infinit version as a reference.
    /// Thus, whenever the administrator updates the Infinit software, its
    /// version along with the supported format are recorded in the descriptor
    /// which is upgraded and re-sealed. These reference formats are used
    /// whenever a node receives a block in an unkown format e.g a format
    /// higher than the one supported by the user's Infinit version. In this
    /// case, either the version of Infinit is too old to understand this
    /// format or the block is invalid: embedding a format 12345 for example
    /// though this format is supported by no Infinit version. Should a node
    /// receive such a block, it would check the format against the reference
    /// format of the given block type e.g ContentHashBlock. If the format is
    /// higher that the reference, the node would discard the block as invalid
    /// and would continue. Otherwise, a warning would be displayed asking the
    /// user to update the Infinit software in order to be able to understand
    /// the latest formats.
    ///
    /// Noteworthy is that formats should evolve quite rarely. Thus, the
    /// impropriety of forcing the user to update the Infinit software should
    /// not be too important.
    struct Data:
      public elle::serialize::DynamicFormat<Data>,
      public elle::Printable
    {
      /*--------.
      | Friends |
      `--------*/
    public:
      // Required for the format 0.
      friend class Descriptor;

      /*------.
      | Types |
      `------*/
    public:
      typedef elle::serialize::DynamicFormat<Data> DynamicFormat;
      // XXX required so as to provide a specific serializer: to remove
      //     when the serialization mechanism will handle polymorphic
      //     types by embedding an identifier for the reconstruction on the
      //     other end.
      class Vector:
        public std::vector<std::unique_ptr<nucleus::proton::Block>>
      {
      public:
        template <typename... T>
        Vector(T&&... args):
          std::vector<std::unique_ptr<nucleus::proton::Block>>(
            std::forward<T>(args)...)
        {}
      };

      /*-------------.
      | Construction |
      `-------------*/
    public:
      Data() {} // XXX[to remove when the new serialization will be fixed]
      /// Construct a data section from the given elements.
      Data(elle::String name,
           hole::Openness openness,
           horizon::Policy policy,
           Vector blocks,
           elle::Version version,
           elle::serialize::Format format_block,
           elle::serialize::Format format_content_hash_block,
           elle::serialize::Format format_contents,
           elle::serialize::Format format_immutable_block,
           elle::serialize::Format format_imprint_block,
           elle::serialize::Format format_mutable_block,
           elle::serialize::Format format_owner_key_block,
           elle::serialize::Format format_public_key_block,
           elle::serialize::Format format_access,
           elle::serialize::Format format_attributes,
           elle::serialize::Format format_catalog,
           elle::serialize::Format format_data,
           elle::serialize::Format format_ensemble,
           elle::serialize::Format format_group,
           elle::serialize::Format format_object,
           elle::serialize::Format format_reference,
           elle::serialize::Format format_user,
           elle::serialize::Format format_identity,
           elle::serialize::Format format_descriptor,
           cryptography::Signature signature);
      /// Construct a data section based on the passed elements which will
      /// be signed with the given private key.
      ///
      /// Note that, through this helper, the administrator must provide a
      /// sign(data) method.
      template <typename T>
      Data(elle::String name,
           hole::Openness openness,
           horizon::Policy policy,
           Vector blocks,
           elle::Version version,
           elle::serialize::Format format_block,
           elle::serialize::Format format_content_hash_block,
           elle::serialize::Format format_contents,
           elle::serialize::Format format_immutable_block,
           elle::serialize::Format format_imprint_block,
           elle::serialize::Format format_mutable_block,
           elle::serialize::Format format_owner_key_block,
           elle::serialize::Format format_public_key_block,
           elle::serialize::Format format_access,
           elle::serialize::Format format_attributes,
           elle::serialize::Format format_catalog,
           elle::serialize::Format format_data,
           elle::serialize::Format format_ensemble,
           elle::serialize::Format format_group,
           elle::serialize::Format format_object,
           elle::serialize::Format format_reference,
           elle::serialize::Format format_user,
           elle::serialize::Format format_identity,
           elle::serialize::Format format_descriptor,
           T const& administrator);
      Data(Data&& other);
      ELLE_SERIALIZE_CONSTRUCT_DECLARE(Data);
    private:
      Data(Data const&);

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Return true if the data section is valid i.e has been signed with
      /// the administrator private key, false otherwise.
      ///
      /// Note that this method requires the _administrator_ to provide a
      /// verify(signature, data) method.
      template <typename T>
      elle::Boolean
      validate(T const& administrator) const;

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
      /// A human-readable name for the network.
      ELLE_ATTRIBUTE_R(elle::String, name);
      /// The way other computers, i.e nodes, can connect to this network.
      ELLE_ATTRIBUTE_R(hole::Openness, openness);
      /// The default sharing behavior for every file system object
      /// being created.
      ELLE_ATTRIBUTE_R(horizon::Policy, policy);
      /// The set of initial blocks in the storage layer.
      ELLE_ATTRIBUTE_R(Vector, blocks);
      /// The set of IP addresses referencing the stable nodes of the network
      /// which can be used for boostraping i.e discovering the network.
      // XXX ELLE_ATTRIBUTE_R(XXX, XXX);
      /// The most recent version of the Infinit software supported by the
      /// network.
      ///
      /// Nodes with a more recent version should operate with the formats
      /// below so as to be sure not to create blocks in formats that other
      /// nodes will not be able to understand. Likewise, any node whose
      /// software is behind should immediately be updated so as to be sure
      /// to understand every format being created.
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
      ///
      /// These are the elements which must be signed by the network
      /// administrator.
      cryptography::Digest
      hash(elle::String const& name,
           hole::Openness const& openness,
           horizon::Policy const& policy,
           Data::Vector const& blocks,
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
      /// Compatibility with format 0.
      cryptography::Digest
      hash_0(elle::String const& name,
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

# include <infinit/Descriptor.hxx>

#endif
