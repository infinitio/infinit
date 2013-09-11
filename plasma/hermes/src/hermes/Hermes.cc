#include <plasma/hermes/src/hermes/Hermes.hh>
// TODO: Correct include path.

using namespace std;

namespace plasma
{
  namespace hermes
  {
    Hermes::Hermes(reactor::Scheduler& sched, int port, std::string base_path):
      _sched(sched),
      _clerk(base_path),
      _dis(sched, _clerk, port)
    {}

    void
    Hermes::run()
    {
      _dis.run();
    }
  }
}
