#include <infinit/oracles/hermes/HermesClient.hh>
#include <elle/log.hh>

namespace oracles
{
  namespace hermes
  {
    HermesClient::HermesClient(TID transaction_id,
                               reactor::Scheduler& sched,
                               const char* host,
                               const int port):
      _tid(transaction_id),
      _socket(new reactor::network::TCPSocket(sched, host, port)),
      _seria(new infinit::protocol::Serializer(sched, *_socket)),
      _channels(new infinit::protocol::ChanneledStream(sched, *_seria))
    {}

    HermesClient::~HermesClient()
    {
      delete _channels;
      delete _seria;
      delete _socket;
    }

    void
    HermesClient::upload(boost::filesystem::path const& snaploc)
    {
      std::string strsl = snaploc.string();
      frete::TransferSnapshot snap(elle::serialize::from_file(strsl));

      oracles::hermes::HermesRPC handler(*_channels);
      handler.ident(_tid);

      for (auto& file : snap.transfers())
      {
        if (file.second.complete() or not file.second.file_exists())
          continue;

        std::ifstream f(file.second.path());

        while (file.second.progress() != file.second.file_size())
        {
          Size tr = 500;

          // In the case of a ConnexionClosed interruption indicating that the
          // recipient has reconnected, the last block might be stored on the
          // server, and the snapshot increment progress not updated. However,
          // this is not a problem since the Hermes server is flexible.
          // XXX: Handle the ConnectionClosed exception in a better way.

          elle::Buffer ret(tr);
          f.read(reinterpret_cast<char*>(ret.mutable_contents()), tr);
          if (handler.store(file.first, file.second.progress(), ret) == tr)
            snap.increment_progress(file.first, tr);
        }
      }
    }
  }
}
