#ifndef SURFACE_GAP_METRICS_HH
# define SURFACE_GAP_METRICS_HH

# include <metrics/Key.hh>
# include <metrics/Reporter.hh>
# include <surface/gap/Exception.hh>

// XXX Rename that macro with the right prefix
# define CATCH_FAILURE_TO_METRICS(pkey, prefix)                               \
  catch (std::exception const&)                                               \
  {                                                                           \
    this->_reporter[pkey].store(                                              \
      prefix + std::string{".failed"},                                        \
      {{metrics::Key::value, elle::exception_string()}});                     \
    throw;                                                                    \
  }                                                                           \
/**/

// XXX Remove this using (or put it in a namespace)
using MKey = ::metrics::Key;

#endif
