#include "challenge.hh"
#include "metalib.hh"

#include <cryptography/challenge.hh>
#include <cryptography/KeyPair.hh>
#include <elle/serialize/insert.hh>
#include <elle/serialize/extract.hh>

// THIS IS TEMPORARY. IN THE FUTURE, THE KEYPAIR WILL BE PART OF A CERTIFICATE.
static
infinit::cryptography::KeyPair const&
keypair()
{
  static infinit::cryptography::KeyPair challenger(
    elle::serialize::from_file("meta_keypair"));
  return challenger;
}

static
std::pair<std::string, std::string>
generate_challenge(std::string const& challengee_K_base64)
{
  elle::String nonce = infinit::cryptography::challenge::nonce();

  auto K_extractor =
    elle::serialize::from_string<elle::serialize::InputBase64Archive>(
      challengee_K_base64);
  infinit::cryptography::PublicKey challengee_K(K_extractor);

  elle::String challenge =
    infinit::cryptography::challenge::create(keypair().K(),
                                             challengee_K,
                                             nonce);
  return std::make_pair(challenge, nonce);
}

extern "C"
PyObject*
metalib_generate_challenge(PyObject* self,
                           PyObject* args)
{
  (void) self;
  char const* challengee_K_base64 = nullptr;

  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args,
                        "s:generate_challenge",
                        &challengee_K_base64))
    return nullptr;

  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
  {
    ret = PyTuple_New(2);
    auto challenge_nonce = generate_challenge(challengee_K_base64);

    // WARNING: restore state before setting exception !
    PyEval_RestoreThread(_save);

    PyTuple_SetItem(ret, 0, PyString_FromString(challenge_nonce.first.c_str()));
    PyTuple_SetItem(ret, 1, PyString_FromString(challenge_nonce.second.c_str()));
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
check_challenge(std::string const& response,
                std::string const& nonce)
{
  return infinit::cryptography::challenge::check(response,
                                                 keypair().k(),
                                                 nonce);
}

extern "C"
PyObject*
metalib_verify_challenge(PyObject* self,
                         PyObject* args)
{
  (void) self;
  char const* response = nullptr;
  char const* nonce = nullptr;

  PyObject* ret = nullptr;

  if (!PyArg_ParseTuple(args,
                        "ss:check_challenge",
                        &response,
                        &nonce))
    return nullptr;

  PyThreadState *_save;
  _save = PyEval_SaveThread();

  try
  {
    bool res = check_challenge(response, nonce);

    // WARNING: restore state before setting exception !
    PyEval_RestoreThread(_save);

    ret = Py_BuildValue("b", res);
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
