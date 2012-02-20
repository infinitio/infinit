//
// ---------- header ----------------------------------------------------------
//
// project       oracle/disciples/metalib
//
// license       infinit
//
// author        Raphael Londeix   [Mon 20 Feb 2012 03:17:55 PM CET]
//

#include "elle/cryptography/KeyPair.hh"
#include "elle/io/Path.hh"
#include "elle/core/String.hh"

#include "lune/Identity.hh"

#include "metalib.hh"
#include "identity.hh"


///
/// this method creates a new user by generating a new key pair and
/// storing a user block.
///
static lune::Identity create_identity(elle::String const& authority_file,
                                      elle::String const& authority_password,
                                      elle::String const& login,
                                      elle::String const& password)
{
  elle::KeyPair       pair;
  lune::Authority     authority;
  elle::Path          authority_path;
  lune::Identity      identity;

  // check the argument.
  if (login.empty() == true)
    throw std::runtime_error("unable to create a user without a user name");

  if (authority_path.Create(authority_file) == elle::StatusError)
    throw std::runtime_error("unable to create authority path");

  // load the authority file
  if (authority.Load(authority_path) == elle::StatusError)
    throw std::runtime_error("unable to load the authority file");

  // decrypt the authority.
  if (authority.Decrypt(authority_password) == elle::StatusError)
    throw std::runtime_error("unable to decrypt the authority");

  // generate a key pair.
  if (pair.Generate() == elle::StatusError)
    throw std::runtime_error("unable to generate the key pair");

  // create the identity.
  if (identity.Create(login, pair) == elle::StatusError)
    throw std::runtime_error("unable to create the identity");

  // encrypt the identity.
  if (identity.Encrypt(password) == elle::StatusError)
    throw std::runtime_error("unable to encrypt the identity");

  // seal the identity.
  if (identity.Seal(authority) == elle::StatusError)
    throw std::runtime_error("unable to seal the identity");

  return identity;
}

extern "C" PyObject* metalib_generate_identity(PyObject* self, PyObject* args)
{
  char const* login = nullptr,
            * password = nullptr,
            * auth_path = nullptr,
            * auth_password = nullptr;
  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args, "ssss:generate_identity", &login, &password, &auth_path, &auth_password))
    return nullptr;
  if (!login || !password)
    return nullptr;


  Py_BEGIN_ALLOW_THREADS;

  try
  {
    auto identity = create_identity(auth_path, auth_password, login, password);
    elle::String all, pub;
    if (identity.Save(all) != elle::StatusError &&
        identity.pair.k.Save(pub) != elle::StatusError)
      {
        ret = Py_BuildValue("(ss)", all.c_str(), pub.c_str());
      }
  }
  catch (std::exception const& err)
  {
    show();
    PyErr_SetString(metalib_MetaError, err.what());
  }

  Py_END_ALLOW_THREADS;

  return ret;
}

