#include <elle/io/File.hh>
#include <elle/io/Piece.hh>
#include <elle/io/Path.hh>
#include <elle/os/path.hh>
#include <elle/utility/Factory.hh>

#include <common/common.hh>

#include <cryptography/PrivateKey.hh>
#include <cryptography/KeyPair.hh>
#include <cryptography/oneway.hh>

#include <hole/Openness.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Block.hh>

#include <Infinit.hh>

#include <infinit/Descriptor.hh>
#include <infinit/Identity.hh>

//
// ---------- Descriptor ------------------------------------------------------
//

namespace infinit
{
  /*-------------.
  | Construction |
  `-------------*/

  Descriptor::Descriptor(descriptor::Meta meta,
                         descriptor::Data data):
    _meta(new descriptor::Meta(std::move(meta))),
    _data(new descriptor::Data(std::move(data)))
  {
  }

  Descriptor::Descriptor(Descriptor&& other):
    elle::serialize::DynamicFormat<Descriptor>(std::move(other)),
    _meta(std::move(other._meta)),
    _data(std::move(other._data))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Descriptor)
  {
  }

  /*--------.
  | Methods |
  `--------*/

  descriptor::Meta const&
  Descriptor::meta() const
  {
    ELLE_ASSERT_NEQ(this->_meta, nullptr);

    return (*this->_meta);
  }

  descriptor::Data const&
  Descriptor::data() const
  {
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    return (*this->_data);
  }

  void
  Descriptor::update(descriptor::Data data)
  {
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    this->_data.reset(new descriptor::Data(std::move(data)));
  }

  void
  Descriptor::update(std::unique_ptr<descriptor::Data>&& data)
  {
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    this->_data = std::move(data);
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Descriptor::print(std::ostream& stream) const
  {
    ELLE_ASSERT_NEQ(this->_meta, nullptr);
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    stream << "[" << *this->_meta << ", " << *this->_data << "]";
  }
}

//
// ---------- Meta ------------------------------------------------------------
//

namespace infinit
{
  namespace descriptor
  {
    /*-------------.
    | Construction |
    `-------------*/

    Meta::Meta(cryptography::PublicKey authority_K,
               cryptography::PublicKey administrator_K,
               hole::Model model,
               nucleus::proton::Address root_address,
               std::unique_ptr<nucleus::neutron::Object> root_object,
               nucleus::neutron::Group::Identity everybody_group_identity,
               elle::Boolean history,
               elle::Natural32 extent,
               cryptography::Signature authority_signature,
               Identifier identifier):
      _authority_K(std::move(authority_K)),
      _identifier(std::move(identifier)),
      _administrator_K(std::move(administrator_K)),
      _model(std::move(model)),
      _root_address(std::move(root_address)),
      _root_object(std::move(root_object)),
      _everybody_group_identity(std::move(everybody_group_identity)),
      _history(std::move(history)),
      _extent(std::move(extent)),
      _authority_signature(std::move(authority_signature))
    {
      ELLE_ASSERT_NEQ(this->_root_object, nullptr);
    }

    Meta::Meta(Meta&& other):
      elle::serialize::DynamicFormat<Meta>(std::move(other)),
      _authority_K(std::move(other._authority_K)),
      _identifier(std::move(other._identifier)),
      _administrator_K(std::move(other._administrator_K)),
      _model(std::move(other._model)),
      _root_address(std::move(other._root_address)),
      _root_object(std::move(other._root_object)),
      _everybody_group_identity(std::move(other._everybody_group_identity)),
      _history(std::move(other._history)),
      _extent(std::move(other._extent)),
      _authority_signature(std::move(other._authority_signature))
    {
      ELLE_ASSERT_NEQ(this->_root_object, nullptr);
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(Meta,
                                    _authority_K,
                                    _identifier,
                                    _administrator_K,
                                    _model,
                                    _root_address,
                                    _everybody_group_identity,
                                    _authority_signature)
    {
    }

    /*--------.
    | Methods |
    `--------*/

    nucleus::neutron::Subject const&
    Meta::everybody_group_subject() const
    {
      // Create the subject corresponding to the everybody group, if necessary.
      // Note that this subject will never be serialized but is used to ease
      // the process of access control since most methods manipulate subjects.
      if (this->_everybody_group_subject == nullptr)
        this->_everybody_group_subject.reset(
          new nucleus::neutron::Subject(this->_everybody_group_identity));

      return (*this->_everybody_group_subject);
    }

    nucleus::neutron::Object const&
    Meta::root_object() const
    {
      ELLE_ASSERT_NEQ(this->_root_object, nullptr);

      return (*this->_root_object);
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Meta::print(std::ostream& stream) const
    {
      stream << this->_administrator_K << ", "
             << this->_model << ", "
             << this->_root_address << ", "
             << this->_history << ", "
             << this->_extent << ")";
    }

    namespace meta
    {
      /*----------.
      | Functions |
      `----------*/

      cryptography::Digest
      hash(Identifier const& identifier,
           cryptography::PublicKey const& administrator_K,
           hole::Model const& model,
           nucleus::proton::Address const& root_address,
           std::unique_ptr<nucleus::neutron::Object> const& root_object,
           nucleus::neutron::Group::Identity const& everybody_group_identity,
           elle::Boolean history,
           elle::Natural32 extent)
      {
        return (cryptography::oneway::hash(
                  elle::serialize::make_tuple(identifier,
                                              administrator_K,
                                              model,
                                              root_address,
                                              root_object,
                                              everybody_group_identity,
                                              history,
                                              extent),
                  cryptography::KeyPair::oneway_algorithm));
      }
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
    /*-------------.
    | Construction |
    `-------------*/

    Data::Data(elle::String description,
               hole::Openness openness,
               horizon::Policy policy,
               Vector blocks,
               std::vector<Endpoint> nodes,
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
               cryptography::Signature administrator_signature):
      _description(std::move(description)),
      _openness(std::move(openness)),
      _policy(std::move(policy)),
      _blocks(std::move(blocks)),
      _nodes(std::move(nodes)),
      _version(std::move(version)),
      _format_block(std::move(format_block)),
      _format_content_hash_block(std::move(format_content_hash_block)),
      _format_contents(std::move(format_contents)),
      _format_immutable_block(std::move(format_immutable_block)),
      _format_imprint_block(std::move(format_imprint_block)),
      _format_mutable_block(std::move(format_mutable_block)),
      _format_owner_key_block(std::move(format_owner_key_block)),
      _format_public_key_block(std::move(format_public_key_block)),
      _format_access(std::move(format_access)),
      _format_attributes(std::move(format_attributes)),
      _format_catalog(std::move(format_catalog)),
      _format_data(std::move(format_data)),
      _format_ensemble(std::move(format_ensemble)),
      _format_group(std::move(format_group)),
      _format_object(std::move(format_object)),
      _format_reference(std::move(format_reference)),
      _format_user(std::move(format_user)),
      _format_identity(std::move(format_identity)),
      _format_descriptor(std::move(format_descriptor)),
      _administrator_signature(std::move(administrator_signature))
    {
    }

    Data::Data(Data&& other):
      elle::serialize::DynamicFormat<Data>(std::move(other)),
      _description(std::move(other._description)),
      _openness(std::move(other._openness)),
      _policy(std::move(other._policy)),
      _blocks(std::move(other._blocks)),
      _nodes(std::move(other._nodes)),
      _version(std::move(other._version)),
      _format_block(std::move(other._format_block)),
      _format_content_hash_block(std::move(other._format_content_hash_block)),
      _format_contents(std::move(other._format_contents)),
      _format_immutable_block(std::move(other._format_immutable_block)),
      _format_imprint_block(std::move(other._format_imprint_block)),
      _format_mutable_block(std::move(other._format_mutable_block)),
      _format_owner_key_block(std::move(other._format_owner_key_block)),
      _format_public_key_block(std::move(other._format_public_key_block)),
      _format_access(std::move(other._format_access)),
      _format_attributes(std::move(other._format_attributes)),
      _format_catalog(std::move(other._format_catalog)),
      _format_data(std::move(other._format_data)),
      _format_ensemble(std::move(other._format_ensemble)),
      _format_group(std::move(other._format_group)),
      _format_object(std::move(other._format_object)),
      _format_reference(std::move(other._format_reference)),
      _format_user(std::move(other._format_user)),
      _format_identity(std::move(other._format_identity)),
      _format_descriptor(std::move(other._format_descriptor)),
      _administrator_signature(std::move(other._administrator_signature))
    {
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(Data,
                                    _version,
                                    _format_block,
                                    _format_content_hash_block,
                                    _format_contents,
                                    _format_immutable_block,
                                    _format_imprint_block,
                                    _format_mutable_block,
                                    _format_owner_key_block,
                                    _format_public_key_block,
                                    _format_access,
                                    _format_attributes,
                                    _format_catalog,
                                    _format_data,
                                    _format_ensemble,
                                    _format_group,
                                    _format_object,
                                    _format_reference,
                                    _format_user,
                                    _format_identity,
                                    _format_descriptor,
                                    _administrator_signature)
    {
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Data::print(std::ostream& stream) const
    {
      stream << this->_description << "("
             << this->_openness << ", "
             << this->_policy << ", ";

      elle::Boolean first = true;
      stream << "[";
      for (auto const& pair: this->_nodes)
      {
        if (!first)
          stream << ", ";
        stream << pair.first << ":" << pair.second;
        first = false;
      }
      stream << "], ";

      stream << this->_blocks.size() << ", "
             << this->_version << ")";
    }

    namespace data
    {
      /*----------.
      | Functions |
      `----------*/

      cryptography::Digest
      hash(elle::String const& description,
           hole::Openness const& openness,
           horizon::Policy const& policy,
           Data::Vector const& blocks,
           std::vector<Data::Endpoint> const& nodes,
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
           elle::serialize::Format const& format_descriptor)
      {
        return (cryptography::oneway::hash(
                  elle::serialize::make_tuple(
                    description,
                    openness,
                    policy,
                    blocks,
                    nodes,
                    version,
                    format_block,
                    format_content_hash_block,
                    format_contents,
                    format_immutable_block,
                    format_imprint_block,
                    format_mutable_block,
                    format_owner_key_block,
                    format_public_key_block,
                    format_access,
                    format_attributes,
                    format_catalog,
                    format_data,
                    format_ensemble,
                    format_group,
                    format_object,
                    format_reference,
                    format_user,
                    format_identity,
                    format_descriptor),
                  cryptography::KeyPair::oneway_algorithm));
      }
    }
  }
}
