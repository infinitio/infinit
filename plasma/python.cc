#include <boost/python.hpp>

void export_trophonius();
void export_meta();

BOOST_PYTHON_MODULE(plasma)
{
  export_meta();
  export_trophonius();
}
