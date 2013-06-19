#include "metalib.hh"

#include <iostream>

#include <lune/Lune.hh>

#include "identity.hh"
#include "passport.hh"
#include "network.hh"

// XXX When Qt is out, remove this
#ifdef slots
# undef slots
#endif
#ifdef slot
# undef slot
#endif

/// metalib error
PyObject* metalib_MetaError = nullptr;

/// metalib methods
static PyMethodDef _metalib_methods[] = {
  {
    "generate_identity",
    &metalib_generate_identity,
    METH_VARARGS,
    "Generate an identity.\n"
    "\n"
    ":param login: The user unique id (its email)\n"
    ":param password: The user password\n"
    ":param authority_file: The path to the authority file\n"
    ":param authority_password: Password to decrypt the authority file\n"
    ":rtype: A tuple of two string\n"
    ":return: a pair of strings: the whole identity and the public part of the identity\n"
  },
  {
    "generate_passport",
    &metalib_generate_passport,
    METH_VARARGS,
    "Generate a passport.\n"
    "\n"
    ":param id: Passport id\n"
    ":param name: Passport name\n"
    ":param user_pubkey: User public key\n"
    ":param authority_file: The path to the authority file\n"
    ":param authority_password: Password to decrypt the authority file\n"
    ":rtype: String\n"
    ":return: The encoded passport\n"
  },
  {
    "generate_network_descriptor",
    &metalib_generate_network_descriptor,
    METH_VARARGS,
    "Generate a network descriptor\n"
    "\n"
    ":param network_id: The network unique identifier\n"
    ":param user_identity: The user identity, in a decrypted form\n"
    ":param model: Then network model name\n"
    ":param directory_address: The base64 encoded root block address\n"
    ":param group_address: The base64 encoded group address\n"
    ":param authority_file: The path to the authority file\n"
    ":param authority_password: Password to decrypt the authority file\n"
    ":rtype: String\n"
    ":return: base64 encoded network descriptor\n"
  },
  {
    "check_root_directory_signature",
    &metalib_check_root_directory_signature,
    METH_VARARGS,
    "Check wether or not the root block is valid.\n"
    "\n"
    ":param root_block: The base64 encoded root block\n"
    ":param root_address: The base64 encoded root address\n"
    ":param group_block: The base64 encoded group block\n"
    ":param group_address: The base64 encoded group address\n"
    ":param public_key: The base64 encoded public key\n"
    ":rtype: Boolean\n"
    ":return: Wether or not the root_block is valid\n"
  },
  {NULL, NULL, 0, NULL}        /* Sentinel */
};


char const* const _metalib_doc =
  "User friendly interface for infinit files\n"
;

// Pacify -Wmissing-declarations.
PyMODINIT_FUNC initmetalib(void);

PyMODINIT_FUNC initmetalib(void)
{
  fprintf(stderr, "init module\n");

  fprintf(stderr, "elle initialized\n");
  if (lune::Lune::Initialize() == elle::Status::Error)
    {
      std::cerr << "Cannot initialize lune\n";
      return;
    }
  fprintf(stderr, "lune initialized\n");

  PyObject* module = Py_InitModule3(
      METALIB_MOD_NAME,
      _metalib_methods,
      _metalib_doc
  );

  fprintf(stderr, "python module initialized\n");
  char error_name[] = "metalib.MetaError";

  metalib_MetaError = PyErr_NewException(error_name, nullptr, nullptr);
  Py_INCREF(metalib_MetaError);
  PyModule_AddObject(module, error_name, metalib_MetaError);
}
