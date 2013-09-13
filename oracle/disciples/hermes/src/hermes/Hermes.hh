#ifndef ORACLE_DISCIPLES_HERMES_HH
# define ORACLE_DISCIPLES_HERMES_HH

# include <reactor/scheduler.hh>

# include <oracle/disciples/hermes/src/hermes/Clerk.hh>
# include <oracle/disciples/hermes/src/hermes/Dispatcher.hh>

namespace oracle
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
      Dispatcher _dis;
    };
  }
}

#endif // !ORACLE_DISCIPLES_HERMES_HH
