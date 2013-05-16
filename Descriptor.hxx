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
