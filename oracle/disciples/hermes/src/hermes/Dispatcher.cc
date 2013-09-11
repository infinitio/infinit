#include <oracle/disciples/hermes/src/hermes/Dispatcher.hh>
// TODO: Correct include path.

namespace oracle
{
  namespace hermes
  {
    // TODO: Implement this in a cleaner way, without a global variable.
    static Clerk* global_clerk = nullptr;

    Dispatcher::Dispatcher(reactor::Scheduler& sched, Clerk& clerk, int port):
      _sched(sched),
      _serv(sched),
      _port(port)
    {
      global_clerk = &clerk;
    }

    void
    Dispatcher::run()
    {
      _serv.listen(_port);

      // Run an RPC at each connection.
      while (true)
      {
        infinit::protocol::Serializer s(_sched, *_serv.accept());
        infinit::protocol::ChanneledStream channels(_sched, s);
        Handler(channels).run();
      }
    }

    Handler::Handler(infinit::protocol::ChanneledStream& channels):
      infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                             elle::serialize::OutputBinaryArchive>(channels),
      store("store", *this),
      serve("serve", *this)
    {
      this->store = &store_wrapper;
      this->serve = &serve_wrapper;
    }

    Size
    Handler::store_wrapper(FileID id, Offset off, elle::Buffer& buff)
    {
      return global_clerk->store(ChunkMeta(id, off), buff);
    }

    elle::Buffer
    Handler::serve_wrapper(FileID id, Offset off)
    {
      return global_clerk->serve(ChunkMeta(id, off));
    }
  }
}

