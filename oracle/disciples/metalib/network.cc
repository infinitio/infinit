#include "metalib.hh"

#include <elle/io/Path.hh>
#include <elle/types.hh>
#include <elle/io/Unique.hh>
#include <elle/serialize/extract.hh>

#include <cryptography/KeyPair.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <hole/Model.hh>

#include <lune/Identity.hh>
#include <lune/Descriptor.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Block.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Trait.hh>

#include <hole/Openness.hh>
#include <hole/Authority.hh>

#include <horizon/Policy.hh>

#include <Infinit.hh>

// XXX When Qt is out, remove this
#ifdef slots
# undef slots
#endif
#ifdef slot
# undef slot
#endif

#include "network.hh"

ELLE_LOG_COMPONENT("infinit.oracle.disciples.metalib.Network");

static
elle::io::Unique
generate_network_descriptor(elle::String const& id,
                            elle::String const& identity_,
                            elle::String const& name,
                            elle::String const& model_name,
                            elle::io::Unique const& directory_address_,
                            elle::io::Unique const& group_address_,
                            elle::String const& authority_file,
                            elle::String const& authority_password)
{
  // XXX should be configurable.
  static horizon::Policy policy = horizon::Policy::accessible;
  static hole::Openness openness = hole::Openness::closed;

  hole::Model model;
  elle::io::Path authority_path;

  if (authority_path.Create(authority_file) == elle::Status::Error)
    throw std::runtime_error("unable to create authority path");

  elle::Authority authority(authority_path);

  if (authority.Decrypt(authority_password) == elle::Status::Error)
    throw std::runtime_error("unable to decrypt the authority");

  if (model.Create(model_name) != elle::Status::Ok)
    throw std::runtime_error("unable to create model");

  nucleus::proton::Address directory_address;
  if (directory_address.Restore(directory_address_) != elle::Status::Ok)
    throw std::runtime_error("Unable to restore directory address");

  nucleus::proton::Address group_address;
  if (group_address.Restore(group_address_) != elle::Status::Ok)
    throw std::runtime_error("Unable to restore group address");

  lune::Identity identity;
  if (identity.Restore(identity_) != elle::Status::Ok)
    throw std::runtime_error("Unable to restore the identity");

  lune::Descriptor descriptor(id,
                              identity.pair().K(),
                              model,
                              directory_address,
                              group_address,
                              name,
                              openness,
                              policy,
                              lune::Descriptor::History,
                              lune::Descriptor::Extent,
                              Infinit::version,
                              authority);

  descriptor.seal(identity.pair().k());

  descriptor.Dump();

  elle::io::Unique unique;
  if (descriptor.Save(unique) != elle::Status::Ok)
    throw std::runtime_error("Unable to save the network descriptor");

  return unique;
}


extern "C"
PyObject*
metalib_generate_network_descriptor(PyObject* self, PyObject* args)
{
  (void) self;
  char const* network_id = nullptr,
            * user_identity = nullptr,
            * network_name = nullptr,
            * network_model = nullptr,
            * directory_address = nullptr,
            * group_address = nullptr,
            * authority_file = nullptr,
            * authority_password = nullptr;
  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args, "ssssssss:generate_network_descriptor",
                        &network_id,
                        &user_identity,
                        &network_name,
                        &network_model,
                        &directory_address,
                        &group_address,
                        &authority_file,
                        &authority_password))
    return nullptr;

  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
    {
      elle::io::Unique descriptor = generate_network_descriptor(
          network_id,
          user_identity,
          network_name,
          network_model,
          directory_address,
          group_address,
          authority_file,
          authority_password
      );

      // WARNING: restore state before setting exception !
      PyEval_RestoreThread(_save);

      ret = Py_BuildValue("s", descriptor.c_str());
    }
  catch (std::exception const& err)
    {
      // WARNING: restore state before setting exception !
      PyEval_RestoreThread(_save);
      char const* error_string = err.what();
      PyErr_SetString(metalib_MetaError, error_string);
    }

  return ret;
}


static
bool
check_root_directory_signature(elle::io::Unique const& root_block_,
                               elle::io::Unique const& root_address_,
                               elle::io::Unique const& group_block_,
                               elle::io::Unique const& group_address_,
                               elle::io::Unique const& public_key)
{
  cryptography::PublicKey       K;

  if (K.Restore(public_key) != elle::Status::Ok)
    throw std::runtime_error("Unable to restore public key");

  // root block ---------------------------------------------------------------
  {
    nucleus::proton::Address root_address;
    if (root_address.Restore(root_address_) == elle::Status::Error)
      throw std::runtime_error("Unable to restore root address");

    nucleus::neutron::Object root_block{
      elle::serialize::from_string<elle::serialize::InputBase64Archive>(
        root_block_)};

    try
      {
        // XXX root_block.validate(root_address, /*fingerprint*/);
        ELLE_WARN("the root block should be validated: assert on radix "
                  "strategy which should be 'value': ask Julien");
      }
    catch (nucleus::Exception const& e)
      {
        return false;
      }

    if (root_block.owner_K() != K)
      return false;
  }


  // group block -------------------------------------------------------------
  {
    nucleus::proton::Address group_address;
    if (group_address.Restore(group_address_) == elle::Status::Error)
      throw std::runtime_error("Unable to restore group address");

    nucleus::neutron::Group group_block{
      elle::serialize::from_string<elle::serialize::InputBase64Archive>(
        group_block_)};

    try
      {
        group_block.validate(group_address);
      }
    catch (nucleus::Exception const& e)
      {
        return false;
      }

    if (group_block.owner_K() != K)
      return false;
  }


  return true;
}


extern "C"
PyObject*
metalib_check_root_directory_signature(PyObject* self,
                                       PyObject* args)
{
  (void) self;

  char const* root_block = nullptr;
  char const* root_address = nullptr;
  char const* group_block = nullptr;
  char const* group_address = nullptr;
  char const* public_key = nullptr;
  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args, "sssss:check_root_directory_signature",
                        &root_block,
                        &root_address,
                        &group_block,
                        &group_address,
                        &public_key))
    return nullptr;

  if (!root_block || !public_key)
    return nullptr;

  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
    {
      bool result = check_root_directory_signature(
          root_block,
          root_address,
          group_block,
          group_address,
          public_key
      );
      // WARNING: restore state before setting exception !
      PyEval_RestoreThread(_save);
      ret = result ? Py_True : Py_False;
      Py_INCREF(ret);
    }
  catch (std::exception const& err)
    {
      // WARNING: restore state before setting exception !
      PyEval_RestoreThread(_save);
      char const* error_string = err.what();
      PyErr_SetString(metalib_MetaError, error_string);
    }

  return ret;
}
