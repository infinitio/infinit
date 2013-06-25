#ifndef ORACLE_DISCIPLES_METALIB_CHALLENGE_HH
# define ORACLE_DISCIPLES_METALIB_CHALLENGE_HH

# include <Python.h>

extern "C"
PyObject*
metalib_generate_challenge(PyObject* self,
                           PyObject* args);

extern "C"
PyObject*
metalib_verify_challenge(PyObject* self,
                         PyObject* args);

#endif
