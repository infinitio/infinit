#include <wrappers/boost/python.hh>
#include "Network.hh"

BOOST_PYTHON_MODULE(_crust)
{
  namespace py = boost::python;
  typedef py::return_value_policy<py::return_by_value> by_value;
  typedef py::return_value_policy<py::copy_const_reference> by_ref;

  py::class_<Network>("Network", boost::python::init<std::string const&>())
    .def("sign", &Network::seal)
    .def("validate", &Network::validate)
    .def("create", &Network::create, by_ref());
}
