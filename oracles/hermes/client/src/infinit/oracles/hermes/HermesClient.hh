#ifndef ORACLES_HERMES_HERMESCLIENT_HH
# define ORACLES_HERMES_HERMESCLIENT_HH

# include <infinit/oracles/hermes/Hermes.hh>

namespace oracle
{
  namespace hermes
  {
    class HermesClient
    {
    public:
      HermesClient(TID transaction_id,
                   reactor::Scheduler::Scheduler& sched,
                   const char* host = "127.0.0.1",
                   const int port = "4242");

    private:
      TID _tid;

      // Socket, Serializer, Stream and RPC stuff.
    private:
      reactor::network::TCPSocket* _socket;
      infinit::protocol::Serializer* _seria;
      infinit::protocol::ChanneledStream* _channels;
      oracles::hermes::HermesRPC* _handler;
    };
  }
}

#endif // !ORACLES_HERMES_HERMESCLIENT_HH
