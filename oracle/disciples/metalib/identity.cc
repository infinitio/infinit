#include "metalib.hh"

#include <elle/types.hh>
#include <elle/io/Path.hh>
#include <elle/serialize/insert.hh>
#include <elle/serialize/Base64Archive.hh>

#include <cryptography/KeyPair.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <hole/Authority.hh>

// XXX When Qt is out, remove this
#ifdef slots
# undef slots
#endif
#ifdef slot
# undef slot
#endif

#include <infinit/Identity.hh>

#include "identity.hh"

///
/// this method creates a new user by generating a new key pair and
/// storing a user block.
///
static infinit::Identity create_identity(elle::String const& id,
                                         elle::String const& authority_file,
                                         elle::String const& authority_password,
                                         elle::String const& login,
                                         elle::String const& password)
{
  elle::io::Path authority_path;

  // check the argument.
  if (login.empty() == true)
    throw std::runtime_error("unable to create a user without a user name");

  if (authority_path.Create(authority_file) == elle::Status::Error)
    throw std::runtime_error("unable to create authority path");

  // Load the authority file.
  elle::Authority authority(authority_path);

  // decrypt the authority.
  if (authority.Decrypt(authority_password) == elle::Status::Error)
    throw std::runtime_error("unable to decrypt the authority");

  cryptography::KeyPair keypair =
    cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa,
                                    infinit::Identity::keypair_length);
  infinit::Identity identity(id, login, keypair, password, authority.k());

  return identity;
}

extern "C"
PyObject*
metalib_generate_identity(PyObject*,
                          PyObject* args)
{
  char const* id = nullptr,
            * login = nullptr,
            * password = nullptr,
            * auth_path = nullptr,
            * auth_password = nullptr;
  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args, "sssss:generate_identity",
                        &id, &login, &password, &auth_path, &auth_password))
    return nullptr;

  if (!id || !login || !password || !auth_path || !auth_password)
    return nullptr;


  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
    {
      auto identity =
        create_identity(id, auth_path, auth_password, login, password);

      elle::String all;
      elle::serialize::to_string<elle::serialize::OutputBase64Archive>(all) <<
        identity;

      auto keypair = identity.decrypt(password);
      elle::String pub;
      elle::serialize::to_string<elle::serialize::OutputBase64Archive>(pub) <<
        keypair.K();

      // WARNING: restore state before setting exception !
      PyEval_RestoreThread(_save);

      ret = Py_BuildValue("(ss)", all.c_str(), pub.c_str());
    }
  catch (...)
    {
      // WARNING: restore state before setting exception !
      PyEval_RestoreThread(_save);
      //show();
      PyErr_SetString(metalib_MetaError, elle::exception_string().c_str());
    }

  return ret;
}
