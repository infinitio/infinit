#include "metalib.hh"

#include <elle/io/Path.hh>
#include <elle/types.hh>
#include <elle/io/Unique.hh>
#include <elle/serialize/extract.hh>

#include <cryptography/KeyPair.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <hole/Model.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Block.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Trait.hh>

#include <hole/Openness.hh>

#include <horizon/Policy.hh>

#include <Infinit.hh>
#include <infinit/Descriptor.hh>
#include <infinit/Identity.hh>
#include <infinit/Authority.hh>
#include <infinit/infinit.hh>

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
                            elle::String const& description,
                            elle::String const& model_name,
                            elle::io::Unique const& directory_address_,
                            elle::io::Unique const& group_address_,
                            elle::String const& authority_path,
                            elle::String const& authority_password)
{
  // XXX should be configurable.
  static horizon::Policy policy = horizon::Policy::accessible;
  static hole::Openness openness = hole::Openness::closed;

  hole::Model model;

  infinit::Authority authority(elle::serialize::from_file(authority_path));

  cryptography::PrivateKey authority_k = authority.decrypt(authority_password);

  if (model.Create(model_name) != elle::Status::Ok)
    throw std::runtime_error("unable to create model");

  nucleus::proton::Address directory_address;
  if (directory_address.Restore(directory_address_) != elle::Status::Ok)
    throw std::runtime_error("Unable to restore directory address");

  nucleus::proton::Address group_address;
  if (group_address.Restore(group_address_) != elle::Status::Ok)
    throw std::runtime_error("Unable to restore group address");

  auto extractor =
    elle::serialize::from_string<
      elle::serialize::InputBase64Archive>(identity_);
  infinit::Identity identity(extractor);

  /* XXX[to adapt to the new descriptor]
  Descriptor descriptor(id,
                        identity.pair().K(),
                        model,
                        directory_address,
                        group_address,
                        description,
                        openness,
                        policy,
                        false,
                        1048576,
                        Infinit::version,
                        authority_k);

  descriptor.seal(identity.pair().k());

  descriptor.Dump();

  elle::io::Unique unique;
  if (descriptor.Save(unique) != elle::Status::Ok)
    throw std::runtime_error("Unable to save the network descriptor");

  return unique;
  */
}


extern "C"
PyObject*
metalib_generate_network_descriptor(PyObject* self, PyObject* args)
{
  (void) self;
  char const* network_id = nullptr,
            * user_identity = nullptr,
            * network_description = nullptr,
            * network_model = nullptr,
            * directory_address = nullptr,
            * group_address = nullptr,
            * authority_path = nullptr,
            * authority_password = nullptr;
  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args, "ssssssss:generate_network_descriptor",
                        &network_id,
                        &user_identity,
                        &network_description,
                        &network_model,
                        &directory_address,
                        &group_address,
                        &authority_path,
                        &authority_password))
    return nullptr;

  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
    {
      elle::io::Unique descriptor = generate_network_descriptor(
          network_id,
          user_identity,
          network_description,
          network_model,
          directory_address,
          group_address,
          authority_path,
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


static
infinit::Descriptor
deserialize_descriptor(std::string const& serialized)
                       // std::string const& authority_file = nullptr,
                       // * authority_password = nullptr;)
{
  infinit::Descriptor descriptor{
    elle::serialize::from_string<elle::serialize::InputBase64Archive>(
      serialized)};

//  descriptor.validate(authority);

  return descriptor;
}

extern "C"
PyObject*
metalib_deserialize_network_descriptor(PyObject* self,
                                       PyObject* args)
{
  (void) self;

  char const* descriptor_str = nullptr;
  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args,
                        "s:deserialize_descriptor",
                        &descriptor_str))
    return nullptr;

  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
  {
    auto descriptor = deserialize_descriptor(descriptor_str);

    // WARNING: restore state before setting exception !
    PyEval_RestoreThread(_save);

    ret = PyDict_New();
    PyDict_SetItemString(ret,
                         "id",
                         PyString_FromString(
                           descriptor.meta().identifier().c_str()));
    PyDict_SetItemString(ret,
                         "description",
                         PyString_FromString(
                           descriptor.data().description().c_str()));

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
