#include <boost/python.hpp>

void export_trophonius();
void export_meta();

// Pacify -Wmissing-declaration
extern "C"
{
  PyObject* PyInit_plasma();
}

BOOST_PYTHON_MODULE(plasma)
{
  export_meta();
  export_trophonius();
}
