#include <boost/python.hpp>

#include <elle/python/datetime-converter.hh>

#include <infinit/oracles/apertus/Apertus.hh>

// Pacify -Wmissing-declarations
extern "C"
{
  PyObject* PyInit_server();
}

elle::PluginLoad load_python_bindings(
  elle::python::datetime_converter
  );

BOOST_PYTHON_MODULE(server)
{
  using infinit::oracles::apertus::Apertus;
  boost::python::class_<Apertus,
                        boost::noncopyable>
    ("Apertus",
     boost::python::init<std::string const&,
                         std::string const&,
                         int,
                         std::string const&,
                         int,
                         int,
                         boost::posix_time::time_duration const&,
                         boost::posix_time::time_duration const&>())
    .def("stop", &Apertus::stop)
    .def("wait", &Apertus::wait) // XXX: use Waitable::wait
    .def("port_tcp", &Apertus::port_tcp)
    .def("port_ssl", &Apertus::port_ssl)
    ;
}
