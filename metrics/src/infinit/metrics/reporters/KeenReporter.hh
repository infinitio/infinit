#ifndef INFINIT_METRICS_KEEN_REPORTER_HH
# define INFINIT_METRICS_KEEN_REPORTER_HH

# include <infinit/metrics/reporters/JSONReporter.hh>

namespace infinit
{
  namespace metrics
  {
    class KeenReporter:
      public JSONReporter
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef JSONReporter Super;
      typedef KeenReporter Self;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      KeenReporter();
      virtual
      ~KeenReporter() = default;

    /*-----.
    | Send |
    `-----*/
    protected:
      virtual
      void
      _post(std::string const& destination,
            elle::json::Object data) override;

    /*----------------------.
    | Connection attributes |
    `----------------------*/
    private:
      ELLE_ATTRIBUTE(std::string, base_url);
    };
  }
}

#endif
