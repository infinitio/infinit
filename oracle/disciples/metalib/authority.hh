#ifndef ORACLE_DISCIPLES_METALIB_AUTHORITY_HH
# define ORACLE_DISCIPLES_METALIB_AUTHORITY_HH

# include <Python.h>

extern "C"
PyObject*
metalib_sign(PyObject* self,
             PyObject* args);

extern "C"
PyObject*
metalib_verify(PyObject* self,
               PyObject* args);

#endif
