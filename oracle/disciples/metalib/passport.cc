#include "metalib.hh"

#include <elle/io/Path.hh>
#include <elle/types.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/Base64Archive.hh>

#include <cryptography/random.hh>
#include <cryptography/PublicKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <hole/Passport.hh>

#include <infinit/Authority.hh>

// XXX When Qt is out, remove this
#ifdef slots
# undef slots
#endif
#ifdef slot
# undef slot
#endif

#include "passport.hh"

//
// ---------- functions  ------------------------------------------------------
//

static elle::Passport create_passport(elle::String const& id,
                                      elle::String const& name,
                                      elle::String const& user_K,
                                      elle::String const& authority_path,
                                      elle::String const& authority_password)
{
  infinit::Authority authority(elle::serialize::from_file(authority_path));

  cryptography::PrivateKey authority_k =
    authority.decrypt(authority_password);

  cryptography::PublicKey _user_K(
    elle::serialize::from_string<elle::serialize::InputBase64Archive>(user_K));

  elle::Passport passport(id, name, _user_K, authority_k);

  return passport;
}

extern "C"
PyObject*
metalib_generate_passport(PyObject*, PyObject* args)
{
  PyObject* ret = nullptr;
  char const* id = nullptr,
            * name = nullptr,
            * user_K = nullptr,
            * authority_path = nullptr,
            * authority_password = nullptr;
  if (!PyArg_ParseTuple(args, "sssss:generate_passport",
                        &id, &name, &user_K, &authority_path, &authority_password))
    return nullptr;
  if (!id || !authority_path || !name || !user_K || !authority_password)
    return nullptr;

  Py_BEGIN_ALLOW_THREADS;

  try
    {
      auto passport = create_passport(
        id, name, user_K, authority_path, authority_password
      );
      elle::String passport_string;
      if (passport.Save(passport_string) != elle::Status::Error)
        {
          ret = Py_BuildValue("s", passport_string.c_str());
        }
      else
        {
          PyErr_SetString(
              metalib_MetaError,
              "Cannot convert the passport to string"
          );
        }
    }
  catch(std::exception const& err)
    {
      std::cerr << err.what() << std::endl;
      PyErr_SetString(metalib_MetaError, err.what());
    }

  Py_END_ALLOW_THREADS;

  return ret;
}
