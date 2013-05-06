#ifndef ELLE_METRICS_GOOGLE_HH
# define ELLE_METRICS_GOOGLE_HH

#include <string>
#include <cstdint>

#include "../Reporter.hh"

namespace elle
{
  namespace metrics
  {
    namespace google
    {

      class Service: public elle::metrics::Reporter::Service
      {
      public:
        Service();

      private:
        virtual
        void
        _send(elle::metrics::Reporter::TimeMetricPair const& metric) override;

        virtual
        void
        update_user(std::string const& user) override;

      private:
        std::string _id_file_path;
      };

      std::string
      retrieve_id(std::string const& path);

      // Create a static Reporter with google analytics server.
      void
      register_service(Reporter& reporter);
    }
  }
}

#endif
