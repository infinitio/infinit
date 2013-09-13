#include <oracle/disciples/hermes/src/hermes/Hermes.hh>
// TODO: Correct include path.

using namespace std;

namespace oracle
{
  namespace hermes
  {
    Hermes::Hermes(reactor::Scheduler& sched, int port, std::string base_path):
      _sched(sched),
      _dis(sched, port, base_path)
    {}

    void
    Hermes::run()
    {
      _dis.run();
    }
  }
}
