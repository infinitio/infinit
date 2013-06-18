#include "metalib.hh"

#include <string>
#include <elle/io/Path.hh>
#include <elle/types.hh>
#include <cryptography/Signature.hh>
#include <cryptography/PrivateKey.hh>
#include <cryptography/PublicKey.hh>

#include <elle/serialize/Base64Archive.hh>

#include <elle/serialize/insert.hh>
#include <elle/serialize/extract.hh>

#include <infinit/Authority.hh>

#include "authority.hh"

static
cryptography::KeyPair
authority_keypair(std::string const& authority_path,
                  std::string const& authority_password)
{
  infinit::Authority authority(elle::serialize::from_file(authority_path));

  return cryptography::KeyPair(authority.K(),
                               authority.decrypt(authority_password));
}

static
std::string
sign(std::string const& digest,
     std::string const& authority_path,
     std::string const& authority_password)
{
  auto digest_extractor =
    elle::serialize::from_string<elle::serialize::InputBase64Archive>(digest);
  cryptography::Digest _digest(digest_extractor);

  auto keypair = authority_keypair(authority_path, authority_password);
  cryptography::Signature _signature = keypair.k().sign(_digest);

  std::string signature;
  elle::serialize::to_string<
    elle::serialize::OutputBase64Archive>(signature) << _signature;

  return signature;
}

extern "C"
PyObject*
metalib_sign(PyObject* self,
             PyObject* args)
{
  (void) self;
  char const* digest = nullptr;
  char const* authority_path = nullptr;
  char const* authority_password = nullptr;

  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args,
                        "sss:sign",
                        &digest,
                        &authority_path,
                        &authority_password))
    return nullptr;

  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
  {
    std::string signature = sign(digest,
                                 authority_path,
                                 authority_password);

    // WARNING: restore state before setting exception !
    PyEval_RestoreThread(_save);

    ret = Py_BuildValue("s", signature.c_str());
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
verify(std::string const& digest,
       std::string const& signature,
       std::string const& authority_path,
       std::string const& authority_password)
{
  auto digest_extractor =
    elle::serialize::from_string<elle::serialize::InputBase64Archive>(digest);
  cryptography::Digest _digest(digest_extractor);

  auto signature_extractor =
    elle::serialize::from_string<
      elle::serialize::InputBase64Archive>(signature);
  cryptography::Signature _signature(signature_extractor);

  auto keypair = authority_keypair(authority_path, authority_password);

  return keypair.K().verify(_signature, _digest);
}

extern "C"
PyObject*
metalib_verify(PyObject* self,
               PyObject* args)
{
  (void) self;
  char const* digest = nullptr;
  char const* signature = nullptr;
  char const* authority_path = nullptr;
  char const* authority_password = nullptr;

  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args,
                        "ssss:verify",
                        &digest,
                        &signature,
                        &authority_path,
                        &authority_password))
    return nullptr;

  PyThreadState* _save;
  _save = PyEval_SaveThread();

  try
  {
    bool verified = verify(digest,
                           signature,
                           authority_path,
                           authority_password);

    // WARNING: restore state before setting exception !
    PyEval_RestoreThread(_save);

    ret = Py_BuildValue("b", verified);
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
