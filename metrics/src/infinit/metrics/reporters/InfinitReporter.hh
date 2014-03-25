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
      InfinitReporter(std::string const& url,
                      int port);
      virtual
      ~InfinitReporter() = default;

    /*-----.
    | Send |
    `-----*/
    protected:
      virtual
      std::string
      _url(std::string const& destination) const override;

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
