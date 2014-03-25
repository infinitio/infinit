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
      KeenReporter(std::string const& project,
                   std::string const &key);
      virtual
      ~KeenReporter() = default;

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
    };
  }
}

#endif
