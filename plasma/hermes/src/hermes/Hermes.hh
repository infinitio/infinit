#ifndef INFINIT_PLASMA_HERMES_HH
# define INFINIT_PLASMA_HERMES_HH

# include <reactor/scheduler.hh>

# include <plasma/hermes/src/hermes/Clerk.hh>
# include <plasma/hermes/src/hermes/Dispatcher.hh>

namespace plasma
{
  namespace hermes
  {
    class Hermes
    {
    public:
      Hermes(reactor::Scheduler& sched, int port, std::string base_path);
      void run();

    private:
      reactor::Scheduler& _sched;
      Clerk _clerk;
      Dispatcher _dis;
    };
  }
}

#endif // !INFINIT_PLASMA_HERMES_HH
