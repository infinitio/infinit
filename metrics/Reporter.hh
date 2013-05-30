#ifndef GAP_ANALIZER_HH
# define GAP_ANALIZER_HH

# include <elle/types.hh>
# include <elle/print.hh>
# include <elle/HttpClient.hh>
# include <elle/utility/Time.hh>
# include <queue>
# include <unordered_map>
# include <fstream>
# include <reactor/scheduler.hh>
# include <memory>
# include <thread>

namespace elle
{
  namespace metrics
  {

    //XXX lake of genericity here.
    enum class Key: int
    {
      author,
      count,
      height,
      session,
      size,
      status,
      step,
      tag,
      value,
      width,
      panel,
      input,
    };

    std::ostream&
    operator <<(std::ostream& out,
                Key k);

    class Reporter
    {
      /*------.
      | Types |
      `------*/
    public:
      typedef std::unordered_map<Key, std::string> Metric;
      typedef std::pair<elle::utility::Time, Metric> TimeMetricPair;

    public:
      class Service
      {
      public:
        Service(std::string const& host,
                uint16_t port,
                std::string const& user,
                std::string const& pretty_name);

        virtual
        ~Service();

      private:
        virtual
        void
        _send(TimeMetricPair const& metric) = 0;

      public:
        virtual
        void
        update_user(std::string const& user);

      protected:
        elle::utility::Time _last_sent;
        std::string _tag;
        std::string _user_id;
        std::unique_ptr<elle::HTTPClient> _server;

      public:
         std::string name;

        friend Reporter;
      };

      /*-------------.
      | Construction |
      `-------------*/
      public:
      Reporter();

      virtual
      ~Reporter();

      Reporter(Reporter const&) = delete;
      Reporter(Reporter&&) = delete;

    public:
      /// Start publishing.
      void
      start();

      /// Enqueue data.
      virtual
      void
      store(std::string const& name, Metric const&);

      /// Sugar: Store(name, {})
      void
      store(std::string const& caller);

      /// Sugar: Store(name, {key, value})
      void
      store(std::string const& name,
            Key const& key,
            std::string const& value);

      void
      store(Reporter::TimeMetricPair const& metric);

      void
      add_service(std::unique_ptr<Service> service);

      Reporter::Service&
      service(std::string const& name);

      void
      update_user(std::string const& user);

    protected:
      virtual
      void
      _fallback(Reporter::TimeMetricPair const& metric);

    public:
      static std::string version;
      static std::string user_agent;
      static std::string tag_placeholder;

    protected:
      reactor::Scheduler _flusher_sched;
      std::unique_ptr<boost::asio::io_service::work> _keep_alive;
      std::unique_ptr<std::thread> _run_thread;
      std::ofstream _fallback_stream;
      std::unordered_map<std::string, std::unique_ptr<Service>> _services;
    };

    Reporter&
    reporter();
  }
}

namespace std
{
  template<>
  struct hash<elle::metrics::Key>
  {
  public:
    size_t operator()(const elle::metrics::Key &k) const
    {
      return static_cast<int>(k);
    }
  };
}

#endif
