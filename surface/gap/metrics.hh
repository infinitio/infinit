#ifndef METRICS_HH
# define METRICS_HH

# include <metrics/Reporter.hh>
# include <surface/gap/Exception.hh>

# define CATCH_FAILURE_TO_METRICS(prefix)                                       \
  catch (elle::HTTPException const& e)                                          \
  {                                                                             \
    surface::gap::reporter().store(prefix,                                      \
                                   {{elle::metrics::Key::status, "fail"},       \
                                    {elle::metrics::Key::value, "http" + std::to_string((int) e.code)}}); \
    throw;                                                                      \
  }                                                                             \
  catch (surface::gap::Exception const& e)                                      \
  {                                                                             \
    surface::gap::reporter().store(prefix,                                      \
                                   {{elle::metrics::Key::status, "fail"},       \
                                     {elle::metrics::Key::value, "gap" + std::to_string((int) e.code)}}); \
    throw;                                                                      \
  }                                                                             \
  catch (...)                                                                   \
  {                                                                             \
    surface::gap::reporter().store(prefix,                                      \
                                   {{elle::metrics::Key::status, "fail"},       \
                                     {elle::metrics::Key::value, "unknown"}});  \
    throw;                                                                      \
  } /* */

namespace surface
{
  namespace gap
  {
    using MKey = elle::metrics::Key;

    ///- Metrics ---------------------------------------------------------------
    elle::metrics::Reporter&
    reporter();

    // XXX: While network count is still on GA, we need to keep one reporter
    // to GA.
    elle::metrics::Reporter&
    google_reporter();
  }
}

#endif
