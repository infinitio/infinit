#ifndef METRICS_HH
# define METRICS_HH

# include <metrics/Reporter.hh>
# include <surface/gap/Exception.hh>

# define CATCH_FAILURE_TO_METRICS(prefix)                                       \
  catch (elle::HTTPException const& e)                                          \
  {                                                                             \
    this->_reporter.store(prefix "_fail",                                     \
                          {{elle::metrics::Key::value,                          \
                            "http" + std::to_string((int) e.code)}});           \
    throw;                                                                      \
  }                                                                             \
  catch (surface::gap::Exception const& e)                                      \
  {                                                                             \
    this->_reporter.store(prefix "_fail",                                     \
                          {{elle::metrics::Key::value,                          \
                           "gap" + std::to_string((int) e.code)}});             \
    throw;                                                                      \
  }                                                                             \
  catch (...)                                                                   \
  {                                                                             \
    this->_reporter.store(prefix "_fail",                                     \
                          {{elle::metrics::Key::value, "unknown"}});            \
    throw;                                                                      \
  } /* */

using MKey = elle::metrics::Key;
#endif
