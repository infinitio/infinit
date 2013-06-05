#include <wrappers/boost/python.hh>
#include <surface/crust/Network.hh>

BOOST_PYTHON_MODULE(_crust)
{
  namespace py = boost::python;
  typedef py::return_value_policy<py::return_by_value> by_value;
  typedef py::return_value_policy<py::copy_const_reference> by_cref;

  py::class_<Network>("Network",
                      boost::python::init<std::string const&,
                                          std::string const&,
                                          std::string const&,
                                          std::string const&,
                                          std::string const&,
                                          std::string const&>())
    .def(boost::python::init<std::string const&>())
    .def(boost::python::init<std::string const&,
                             std::string const&,
                             int16_t>())
    .def("list", &Network::list)
//    .def("shelter", &Network::to_shelter)
    .def("store", &Network::store)
    // .def("sign", &Network::seal)
    // .def("validate", &Network::validate)
    // .def("create", &Network::create, by_cref());
    ;
  //py::def("validate", &Network::validate);
}
