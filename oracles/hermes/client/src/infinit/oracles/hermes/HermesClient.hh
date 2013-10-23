#ifndef ORACLES_HERMES_HERMESCLIENT_HH
# define ORACLES_HERMES_HERMESCLIENT_HH

# include <reactor/scheduler.hh>
# include <elle/serialize/BinaryArchive.hh>

# include <frete/Types.hh>
# include <frete/TransferSnapshot.hh>

# include <reactor/network/exception.hh>

# include <infinit/oracles/hermes/Hermes.hh>

namespace oracles
{
  namespace hermes
  {
    class HermesClient
    {
    public:
      HermesClient(TID transaction_id,
                   reactor::Scheduler& sched,
                   const char* host = "127.0.0.1",
                   const int port = 4242);
      ~HermesClient();

    public:
      void
      upload(boost::filesystem::path const& snaploc);

    private:
      TID _tid;

      // Socket, Serializer, Stream and RPC stuff.
    private:
      reactor::network::TCPSocket* _socket;
      infinit::protocol::Serializer* _seria;
      infinit::protocol::ChanneledStream* _channels;
    };
  }
}

#endif // !ORACLES_HERMES_HERMESCLIENT_HH
