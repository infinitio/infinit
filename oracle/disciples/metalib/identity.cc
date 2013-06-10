#include "metalib.hh"

#include <elle/io/Path.hh>
#include <elle/types.hh>

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
  cryptography::KeyPair pair =
    cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa,
                                    infinit::Identity::keypair_length);
  elle::io::Path                    authority_path;
  infinit::Identity                    identity;

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

  // create the identity.
  if (identity.Create(id, login, pair) == elle::Status::Error)
    throw std::runtime_error("unable to create the identity");

  // encrypt the identity.
  if (identity.Encrypt(password) == elle::Status::Error)
    throw std::runtime_error("unable to encrypt the identity");

  // seal the identity.
  if (identity.Seal(authority) == elle::Status::Error)
    throw std::runtime_error("unable to seal the identity");

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
      auto identity = create_identity(id, auth_path, auth_password, login, password);
      elle::String all, pub;
      bool res = (
          identity.Save(all) != elle::Status::Error &&
          identity.pair().K().Save(pub) != elle::Status::Error
      );
      // WARNING: restore state before setting exception !
      PyEval_RestoreThread(_save);

      if (res)
        {
          ret = Py_BuildValue("(ss)", all.c_str(), pub.c_str());
        }
      else
        {
          PyErr_SetString(
              metalib_MetaError,
              "Cannot convert the identity to string"
          );
        }
    }
  catch (std::exception const& err)
    {
      // WARNING: restore state before setting exception !
      PyEval_RestoreThread(_save);
      //show();
      char const* error_string = err.what();
      PyErr_SetString(metalib_MetaError, error_string);
    }

  return ret;
}
