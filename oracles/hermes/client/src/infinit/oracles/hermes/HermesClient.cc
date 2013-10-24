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
          Size tr = 512;

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

    void
    HermesClient::download(boost::filesystem::path const& snaploc, Size off)
    {
      std::string strsl = snaploc.string();
      frete::TransferSnapshot snap(elle::serialize::from_file(strsl));

      // Compare the new offset with the old one.
      find_holes(snap, off);

      oracles::hermes::HermesRPC handler(*_channels);
      handler.ident(_tid);

      while (not _holes.empty())
      {
        auto hole = _holes.begin();
        FileID id(hole->first.first);
        Size off(hole->first.second);
        Size size(hole->second);

        elle::Buffer inc(handler.fetch(id, off, size));

        std::ofstream out(get_path(snap, id));
        out.seekp(off);

        out.write(reinterpret_cast<char*>(inc.mutable_contents()),
                  inc.size());
        out.close();

        _holes.erase(hole);
      }
    }

    void
    HermesClient::find_holes(frete::TransferSnapshot& snap, Size off)
    {
      if (off == snap.progress())
        return;

      Size tot = 0;
      for (const auto& trs : snap.transfers())
      {
        Size tmp = tot;
        tot += trs.second.file_size();

        if (not snap.progress() < tot)
          continue;

        Size loc_off = 0;
        if (snap.progress() > tmp)
          loc_off = snap.progress() % tmp;

        if (off <= tot)
        {
          _holes[{trs.first, loc_off}] = trs.second.file_size() - (tot - off);
          break;
        }
        else
          _holes[{trs.first, loc_off}] = trs.second.file_size() - loc_off;
      }

      // Set the overall progress of the snapshot to the current offset of
      // the sender.
      snap.progress(off);
    }

    std::string
    HermesClient::get_path(frete::TransferSnapshot& snap, FileID id) const
    {
      for (const auto& trs : snap.transfers())
      {
        if (trs.first != id)
          continue;

        return trs.second.path();
      }

      return std::string("/dev/null");
    }
  }
}
