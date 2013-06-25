#ifndef INFINIT_DESCRIPTOR_HXX
# define INFINIT_DESCRIPTOR_HXX

# include <elle/serialize/Pointer.hh>
# include <elle/serialize/PairSerializer.hxx>
# include <elle/serialize/VectorSerializer.hxx>
# include <elle/serialize/StaticFormat.hh>
# include <elle/serialize/insert.hh>
# include <elle/serialize/extract.hh>

# include <cryptography/Signature.hh>

# include <nucleus/proton/Address.hh>
# include <nucleus/neutron/Object.hh>
# include <nucleus/Derivable.hh>

# include <hole/Model.hh>

# include <infinit/Exception.hh>

//
// ---------- Descriptor ------------------------------------------------------
//

namespace infinit
{
  /*--------.
  | Methods |
  `--------*/

  template <typename T>
  elle::Boolean
  Descriptor::validate(T const& authority) const
  {
    ELLE_ASSERT_NEQ(this->_meta, nullptr);
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    if (this->_meta->validate(authority) == false)
      return (false);

    if (this->_data->validate(this->_meta->administrator_K()) == false)
      return (false);

    return (true);
  }
}

/*-----------.
| Serializer |
`-----------*/

ELLE_SERIALIZE_STATIC_FORMAT(infinit::Descriptor, 1);

ELLE_SERIALIZE_SPLIT(infinit::Descriptor);

ELLE_SERIALIZE_SPLIT_SAVE(infinit::Descriptor,
                          archive,
                          value,
                          format)
{
  ELLE_ASSERT_NEQ(value._meta, nullptr);
  ELLE_ASSERT_NEQ(value._data, nullptr);

  switch (format)
  {
    case 0:
    {
      archive << value._meta->_identifier.value();
      archive << value._meta->_administrator_K;
      archive << value._meta->_model;
      archive << value._meta->_root_address;
      archive << value._meta->_everybody_group_identity;
      archive << value._meta->_history;
      archive << value._meta->_extent;
      archive << value._meta->_authority_signature;

      archive << value._data->_description;
      archive << value._data->_openness;
      archive << value._data->_policy;
      archive << value._data->_version;
      archive << value._data->_format_block;
      archive << value._data->_format_content_hash_block;
      archive << value._data->_format_contents;
      archive << value._data->_format_immutable_block;
      archive << value._data->_format_imprint_block;
      archive << value._data->_format_mutable_block;
      archive << value._data->_format_owner_key_block;
      archive << value._data->_format_public_key_block;
      archive << value._data->_format_access;
      archive << value._data->_format_attributes;
      archive << value._data->_format_catalog;
      archive << value._data->_format_data;
      archive << value._data->_format_ensemble;
      archive << value._data->_format_group;
      archive << value._data->_format_object;
      archive << value._data->_format_reference;
      archive << value._data->_format_user;
      archive << value._data->_format_identity;
      archive << value._data->_format_descriptor;
      archive << value._data->_administrator_signature;

      break;
    }
    case 1:
    {
      archive << value._meta;
      archive << value._data;

      break;
    }
    default:
      throw ::infinit::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

ELLE_SERIALIZE_SPLIT_LOAD(infinit::Descriptor,
                          archive,
                          value,
                          format)
{
  ELLE_ASSERT_EQ(value._meta, nullptr);
  ELLE_ASSERT_EQ(value._data, nullptr);

  switch (format)
  {
    case 0:
    {
      value._meta.reset(
        new infinit::descriptor::Meta(elle::serialize::no_init));
      value._meta->infinit::descriptor::Meta::DynamicFormat::version(0);

      // Extract the string-based identifier, create a real
      // identifier and serialize so as to deserialize it in
      // the existing attribute.
      elle::String identifier_value;
      archive >> identifier_value;
      infinit::Identifier identifier(identifier_value);
      elle::String identifier_archive;
      elle::serialize::to_string(identifier_archive) << identifier;
      elle::serialize::from_string(identifier_archive) >>
        value._meta->_identifier;

      // Load the public key of the authority which was used at the time
      // for signing descriptors.
      elle::String authority_K("AAAAAAAAAAAAAgAAwvtjO51oHrOMK/K33ajUm4lnYKWW5dUtyK5Pih7gDrtlpEPy7QWAiYjY8Kinlctca2owzcPXrvFE34gxQE/xz11KLzw4ypn4/ABdzjaTgGoCNLJ2OX99IPk6sEjIHwFxR9YcewD6uED2FQgv4OfOROHaL8hmHzRc0/BxjKwtI6fT7Y/e1v2LMig6r30abqcLrZN+v+3rPHN4hb8n1Jz7kRxRbtglFPLDpvI5LUKEGmu3FPKWWZiJsyFuuLUoC9WsheDDZoHYjyrzMD0k7Sp5YVGBBDthZc6SQDp1ktPSTou1Opk+1IxHp/we1/HNhULvGvr6B1KYZJhb/R55H0k6GcaRQmNEKgiLcF6Z9lA5asK49CC/tlZjKRkXkLBKR9zGIODsweY+O9y3AeGX+Pfk9itPals2egsxc/q2mbRaC9svY2TXMwiSO4EPiedqvpuTKj1KTcRbQrp5mSxG1nzaCGyCmUeGzoBJZLNVJHpytAfwf0oYWfo9NOD9dkKkkL5jxfW3+MOwEx4i0tP3xdKmt6wC6CSXedFVm55oOcz2YgARG3hw0vBdLtl3jxfbXAFjCNnpkMrCEMfjzs5ecFVwhmM8OEPqHpyYJYO/9ipwXAKRPugFzMvoyggSA6G5JfVEwuCgOp2v82ldsKl0sC34/mBeKrJvjaZAXm39f6jTw/sAAAMAAAABAAE=");
      elle::serialize::from_string<elle::serialize::InputBase64Archive>(
        authority_K) >> value._meta->_authority_K;

      archive >> value._meta->_administrator_K;
      archive >> value._meta->_model;
      archive >> value._meta->_root_address;
      archive >> value._meta->_everybody_group_identity;
      archive >> value._meta->_history;
      archive >> value._meta->_extent;
      archive >> value._meta->_authority_signature;

      value._data.reset(
        new infinit::descriptor::Data(elle::serialize::no_init));
      value._data->infinit::descriptor::Data::DynamicFormat::version(0);

      archive >> value._data->_description;
      archive >> value._data->_openness;
      archive >> value._data->_policy;
      archive >> value._data->_version;
      archive >> value._data->_format_block;
      archive >> value._data->_format_content_hash_block;
      archive >> value._data->_format_contents;
      archive >> value._data->_format_immutable_block;
      archive >> value._data->_format_imprint_block;
      archive >> value._data->_format_mutable_block;
      archive >> value._data->_format_owner_key_block;
      archive >> value._data->_format_public_key_block;
      archive >> value._data->_format_access;
      archive >> value._data->_format_attributes;
      archive >> value._data->_format_catalog;
      archive >> value._data->_format_data;
      archive >> value._data->_format_ensemble;
      archive >> value._data->_format_group;
      archive >> value._data->_format_object;
      archive >> value._data->_format_reference;
      archive >> value._data->_format_user;
      archive >> value._data->_format_identity;
      archive >> value._data->_format_descriptor;
      archive >> value._data->_administrator_signature;

      break;
    }
    case 1:
    {
      archive >> value._meta;
      archive >> value._data;

      break;
    }
    default:
      throw ::infinit::Exception(
        elle::sprintf("unknown format '%s'", format));
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

    template <typename T>
    Meta::Meta(cryptography::PublicKey authority_K,
               cryptography::PublicKey administrator_K,
               hole::Model model,
               nucleus::proton::Address root_address,
               std::unique_ptr<nucleus::neutron::Object> root_object,
               nucleus::neutron::Group::Identity everybody_group_identity,
               elle::Boolean history,
               elle::Natural32 extent,
               T const& authority,
               Identifier identifier):
      Meta(std::move(authority_K),
           std::move(administrator_K),
           std::move(model),
           std::move(root_address),
           std::move(root_object),
           std::move(everybody_group_identity),
           std::move(history),
           std::move(extent),
           authority.sign(
             meta::hash(identifier,
                        administrator_K,
                        model,
                        root_address,
                        root_object,
                        everybody_group_identity,
                        history,
                        extent)),
           std::move(identifier))
    {
    }

    /*--------.
    | Methods |
    `--------*/

    template <typename T>
    elle::Boolean
    Meta::validate(T const& authority) const
    {
      ELLE_LOG_COMPONENT("infinit.Descriptor");
      ELLE_TRACE_METHOD(authority);

      return (authority.verify(
                this->_authority_signature,
                meta::hash(this->_identifier,
                           this->_administrator_K,
                           this->_model,
                           this->_root_address,
                           this->_root_object,
                           this->_everybody_group_identity,
                           this->_history,
                           this->_extent)));
    }
  }
}

/*-----------.
| Serializer |
`-----------*/

ELLE_SERIALIZE_SIMPLE(infinit::descriptor::Meta,
                      archive,
                      value,
                      format)
{
  enforce(format == 0, "unknown format");

  archive & value._authority_K;
  archive & value._identifier;
  archive & value._administrator_K;
  archive & value._model;
  archive & value._root_address;
  archive & value._root_object;
  archive & value._everybody_group_identity;
  archive & value._history;
  archive & value._extent;
  archive & value._authority_signature;
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

    template <typename T>
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
               T const& administrator):
      Data(std::move(description),
           std::move(openness),
           std::move(policy),
           std::move(blocks),
           std::move(nodes),
           std::move(version),
           std::move(format_block),
           std::move(format_content_hash_block),
           std::move(format_contents),
           std::move(format_immutable_block),
           std::move(format_imprint_block),
           std::move(format_mutable_block),
           std::move(format_owner_key_block),
           std::move(format_public_key_block),
           std::move(format_access),
           std::move(format_attributes),
           std::move(format_catalog),
           std::move(format_data),
           std::move(format_ensemble),
           std::move(format_group),
           std::move(format_object),
           std::move(format_reference),
           std::move(format_user),
           std::move(format_identity),
           std::move(format_descriptor),
           administrator.sign(
             data::hash(description,
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
                        format_descriptor)))
    {
    }

    /*--------.
    | Methods |
    `--------*/

    template <typename T>
    elle::Boolean
    Data::validate(T const& administrator) const
    {
      ELLE_LOG_COMPONENT("infinit.Descriptor");
      ELLE_TRACE_METHOD(administrator);

      return (administrator.verify(
                this->_administrator_signature,
                data::hash(this->_description,
                           this->_openness,
                           this->_policy,
                           this->_blocks,
                           this->_nodes,
                           this->_version,
                           this->_format_block,
                           this->_format_content_hash_block,
                           this->_format_contents,
                           this->_format_immutable_block,
                           this->_format_imprint_block,
                           this->_format_mutable_block,
                           this->_format_owner_key_block,
                           this->_format_public_key_block,
                           this->_format_access,
                           this->_format_attributes,
                           this->_format_catalog,
                           this->_format_data,
                           this->_format_ensemble,
                           this->_format_group,
                           this->_format_object,
                           this->_format_reference,
                           this->_format_user,
                           this->_format_identity,
                           this->_format_descriptor)));
    }
  }
}

/*-----------.
| Serializer |
`-----------*/

ELLE_SERIALIZE_SIMPLE(infinit::descriptor::Data,
                      archive,
                      value,
                      format)
{
  enforce(format == 0, "unknown format");

  archive & value._description;
  archive & value._openness;
  archive & value._policy;
  archive & value._blocks;
  archive & value._nodes;
  archive & value._version;
  archive & value._format_block;
  archive & value._format_content_hash_block;
  archive & value._format_contents;
  archive & value._format_immutable_block;
  archive & value._format_imprint_block;
  archive & value._format_mutable_block;
  archive & value._format_owner_key_block;
  archive & value._format_public_key_block;
  archive & value._format_access;
  archive & value._format_attributes;
  archive & value._format_catalog;
  archive & value._format_data;
  archive & value._format_ensemble;
  archive & value._format_group;
  archive & value._format_object;
  archive & value._format_reference;
  archive & value._format_user;
  archive & value._format_identity;
  archive & value._format_descriptor;
  archive & value._administrator_signature;
}

// XXX to merge into a simple serializer when the serialization
//     mechanism will be able to support polymorphic types by
//     embedding an identifier for reconstructing it on the other end.
ELLE_SERIALIZE_NO_FORMAT(infinit::descriptor::Data::Vector);

ELLE_SERIALIZE_SPLIT(infinit::descriptor::Data::Vector);

ELLE_SERIALIZE_SPLIT_SAVE(infinit::descriptor::Data::Vector,
                          archive,
                          value,
                          format)
{
  typename Archive::SequenceSizeType size = value.size();
  archive << size;

  for (auto const& pointer: value)
  {
    nucleus::proton::Address address = pointer->bind();
    nucleus::Derivable derivable(address.component(), *pointer);

    archive << derivable;
  }
}

ELLE_SERIALIZE_SPLIT_LOAD(infinit::descriptor::Data::Vector,
                          archive,
                          value,
                          format)
{
  typename Archive::SequenceSizeType size;
  archive >> size;

  for (elle::Natural32 i = 0; i < size; ++i)
  {
    nucleus::Derivable derivable;

    archive >> derivable;

    std::unique_ptr<nucleus::proton::Block> block = derivable.release();

    value.push_back(std::move(block));
  }
}

#endif
