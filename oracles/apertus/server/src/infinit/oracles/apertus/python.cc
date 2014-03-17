#include <boost/python.hpp>

#include <datetime.h> // This is the Python include

#include <elle/python/bindings.cc>

#include <infinit/oracles/apertus/Apertus.hh>

// Pacify -Wmissing-declarations
extern "C"
{
  PyObject* PyInit_apertus();
}

BOOST_PYTHON_MODULE(apertus)
{
  using infinit::oracles::apertus::Apertus;
  boost::python::class_<Apertus,
                        boost::noncopyable>
    ("Trophonius",
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
    ;
}
