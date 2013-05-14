#ifndef LUNE_DESCRIPTOR_HXX
# define LUNE_DESCRIPTOR_HXX

//
// ---------- Descriptor ------------------------------------------------------
//

# include <elle/serialize/Pointer.hh>

# include <cryptography/Signature.hh>

# include <nucleus/proton/Address.hxx>

# include <hole/Model.hh>

ELLE_SERIALIZE_SIMPLE(lune::Descriptor,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  ELLE_ASSERT_NEQ(value._meta, nullptr);

  archive & value._meta->_id;
  archive & value._meta->_administrator_K;
  archive & value._meta->_model;
  archive & value._meta->_root;
  archive & value._meta->_everybody_identity;
  archive & value._meta->_history;
  archive & value._meta->_extent;
  archive & value._meta->_signature;

  ELLE_ASSERT_NEQ(value._data, nullptr);

  archive & value._data->_name;
  archive & value._data->_openness;
  archive & value._data->_policy;
  archive & value._data->_version;
  archive & value._data->_format_block;
  archive & value._data->_format_content_hash_block;
  archive & value._data->_format_contents;
  archive & value._data->_format_immutable_block;
  archive & value._data->_format_imprint_block;
  archive & value._data->_format_mutable_block;
  archive & value._data->_format_owner_key_block;
  archive & value._data->_format_public_key_block;
  archive & value._data->_format_access;
  archive & value._data->_format_attributes;
  archive & value._data->_format_catalog;
  archive & value._data->_format_data;
  archive & value._data->_format_ensemble;
  archive & value._data->_format_group;
  archive & value._data->_format_object;
  archive & value._data->_format_reference;
  archive & value._data->_format_user;
  archive & value._data->_format_identity;
  archive & value._data->_format_descriptor;
  archive & value._data->_signature;
}

#endif
