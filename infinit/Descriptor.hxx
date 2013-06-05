#ifndef INFINIT_DESCRIPTOR_HXX
# define INFINIT_DESCRIPTOR_HXX

# include <elle/serialize/Pointer.hh>
# include <elle/serialize/StaticFormat.hh>
# include <elle/Exception.hh>

# include <cryptography/Signature.hh>

# include <nucleus/proton/Address.hxx>

# include <hole/Model.hh>

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
      value._meta.reset(new infinit::descriptor::Meta);

      archive >> value._meta->_identifier;
      archive >> value._meta->_administrator_K;
      archive >> value._meta->_model;
      archive >> value._meta->_root;
      archive >> value._meta->_everybody_identity;
      archive >> value._meta->_history;
      archive >> value._meta->_extent;
      archive >> value._meta->_signature;

      value._data.reset(new infinit::descriptor::Data);

      archive >> value._data->_name;
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
      archive >> value._data->_signature;

      break;
    }
    case 1:
    {
      archive >> value._meta;
      archive >> value._data;

      break;
    }
    default:
      // XXX ::infinit::Exception
      throw elle::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

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
      archive << value._meta->_identifier;
      archive << value._meta->_administrator_K;
      archive << value._meta->_model;
      archive << value._meta->_root;
      archive << value._meta->_everybody_identity;
      archive << value._meta->_history;
      archive << value._meta->_extent;
      archive << value._meta->_signature;

      archive << value._data->_name;
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
      archive << value._data->_signature;

      break;
    }
    case 1:
    {
      archive << value._meta;
      archive << value._data;

      break;
    }
    default:
      // XXX ::infinit::Exception
      throw elle::Exception(
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
    Meta::Meta(elle::String identifier,
               cryptography::PublicKey administrator_K,
               hole::Model model,
               nucleus::proton::Address root,
               nucleus::neutron::Group::Identity everybody,
               elle::Boolean history,
               elle::Natural32 extent,
               T const& authority):
      Meta(std::move(identifier),
           std::move(administrator_K),
           std::move(model),
           std::move(root),
           std::move(everybody),
           std::move(history),
           std::move(extent),
           authority.sign(
             meta::hash(identifier,
                        administrator_K,
                        model,
                        root,
                        everybody,
                        history,
                        extent)))
    {
    }

    /*--------.
    | Methods |
    `--------*/

    template <typename T>
    elle::Boolean
    Meta::validate(T const& authority) const
    {
      return (authority.verify(
                this->_signature,
                meta::hash(this->_identifier,
                           this->_administrator_K,
                           this->_model,
                           this->_root,
                           this->_everybody_identity,
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
  enforce(format == 0);

  archive & value._identifier;
  archive & value._administrator_K;
  archive & value._model;
  archive & value._root;
  archive & value._everybody_identity;
  archive & value._history;
  archive & value._extent;
  archive & value._signature;
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
    Data::Data(elle::String name,
               hole::Openness openness,
               horizon::Policy policy,
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
      Data(std::move(name),
           std::move(openness),
           std::move(policy),
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
             data::hash(name,
                        openness,
                        policy,
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
      return (administrator.verify(
                this->_signature,
                data::hash(this->_name,
                           this->_openness,
                           this->_policy,
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
  enforce(format == 0);

  archive & value._name;
  archive & value._openness;
  archive & value._policy;
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
  archive & value._signature;
}

#endif
