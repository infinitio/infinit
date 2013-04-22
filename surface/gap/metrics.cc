#include "metrics.hh"

namespace surface
{
  namespace gap
  {
    ///- Metrics ---------------------------------------------------------------
    elle::metrics::Reporter&
    reporter()
    {
      static elle::metrics::Reporter reporter{};

      return reporter;
    }

    // XXX: While network count is still on GA, we need to keep one reporter
    // to GA.
    elle::metrics::Reporter&
    google_reporter()
    {
      static elle::metrics::Reporter reporter{};

      return reporter;
    }
  }
}
