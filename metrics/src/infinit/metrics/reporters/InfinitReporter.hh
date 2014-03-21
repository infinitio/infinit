#ifndef INFINIT_METRICS_INFINIT_REPORTER_HH
# define INFINIT_METRICS_INFINIT_REPORTER_HH

# include <infinit/metrics/reporters/JSONReporter.hh>

namespace infinit
{
  namespace metrics
  {
    class InfinitReporter:
      public JSONReporter
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef JSONReporter Super;
      typedef InfinitReporter Self;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      InfinitReporter();
      virtual
      ~InfinitReporter() = default;

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
      ELLE_ATTRIBUTE(int, port);
    };
  }
}

#endif // INFINIT_METRICS_INFINIT_REPORTER_HH
