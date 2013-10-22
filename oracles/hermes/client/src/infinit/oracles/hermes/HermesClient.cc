#include <infinit/oracles/hermes/HermesClient.hh>

namespace oracle
{
  namespace hermes
  {
    HermesClient::HermesClient(TID transaction_id,
                               reactor::Scheduler::Scheduler& sched,
                               const char* host,
                               const int port):
      _tid(transaction_id),
      _socket(new reactor::network::TCPSocket(sched, host, port)),
      _seria(new infinit::protocol::Serializer(sched, _socket)),
      _channels(new infinit::protocol::ChanneledStream(sched, _seria)),
      _handler(new oracles::hermes::HermesRPC(channels))
    {}

    HermesClient::~HermesClient()
    {
      delete _handler;
      delete _channels;
      delete _seria;
      delete _socket;
    }
  }
}
