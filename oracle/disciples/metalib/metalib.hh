//
// ---------- header ----------------------------------------------------------
//
// project       oracle/disciples/metalib
//
// license       infinit
//
// author        Raphael Londeix   [Mon 20 Feb 2012 02:58:05 PM CET]
//

#ifndef ORACLE_DISCIPLES_METALIB_HH
# define ORACLE_DISCIPLES_METALIB_HH

# include <Python.h>
# if PY_MAJOR_VERSION != 2
#  error "need python2"
# endif

# define METALIB_MOD_NAME "metalib"

extern PyObject* metalib_MetaError;

#endif /* ! METALIB_HH */
