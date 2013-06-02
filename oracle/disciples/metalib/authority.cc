#include "metalib.hh"

#include <string>
#include <elle/io/Path.hh>
#include <hole/Authority.hh>
#include <elle/types.hh>
#include <cryptography/Signature.hh>
#include <cryptography/PrivateKey.hh>
#include <cryptography/PublicKey.hh>

#include <elle/serialize/Base64Archive.hh>

#include <elle/serialize/insert.hh>
#include <elle/serialize/extract.hh>

#include "authority.hh"

static
elle::Authority
authority(std::string const& authority_file,
          std::string const& authority_password)
{
  elle::io::Path authority_path;

  if (authority_path.Create(authority_file) == elle::Status::Error)
    throw std::runtime_error("unable to create authority path");

  elle::Authority authority(authority_path);

  if (authority.Decrypt(authority_password) == elle::Status::Error)
    throw std::runtime_error("unable to decrypt the authority");

  return authority;
}

static
std::string
hash(std::string const& to_hash,
             std::string const& authority_file,
             std::string const& authority_password)
{
  cryptography::Signature signature =
    authority(authority_file, authority_password).k().sign(to_hash);

  std::string hashed;
  elle::serialize::to_string<elle::serialize::OutputBase64Archive>(hashed) << signature;

  return hashed;
}

extern "C"
PyObject*
metalib_sign(PyObject* self,
             PyObject* args)
{
  (void) self;
  char const* to_hash = nullptr;
  char const* authority_file = nullptr;
  char const* authority_password = nullptr;

  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args,
                        "sss:hash",
                        &to_hash,
                        &authority_file,
                        &authority_password))
    return nullptr;

  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
  {
    std::string hashed = hash(to_hash,
                              authority_file,
                              authority_password);

    // WARNING: restore state before setting exception !
    PyEval_RestoreThread(_save);

    ret = Py_BuildValue("s", hashed.c_str());
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
verify(std::string const& hash,
       std::string const& signature_str,
       std::string const& authority_file,
       std::string const& authority_password)
{
  cryptography::Signature signature;
  elle::serialize::from_string<elle::serialize::InputBase64Archive>(hash) >> signature;

  return authority(authority_file, authority_password).K().verify(signature, signature_str);
}

extern "C"
PyObject*
metalib_verify(PyObject* self,
               PyObject* args)
{
  (void) self;
  char const* hash = nullptr;
  char const* signature = nullptr;
  char const* authority_file = nullptr;
  char const* authority_password = nullptr;

  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args,
                        "ssss:verify",
                        &hash,
                        &signature,
                        &authority_file,
                        &authority_password))
    return nullptr;

  PyThreadState* _save;
  _save = PyEval_SaveThread();

  try
  {
    bool verified = verify(hash,
                           signature,
                           authority_file,
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
