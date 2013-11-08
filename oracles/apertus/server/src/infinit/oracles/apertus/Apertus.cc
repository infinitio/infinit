#include <infinit/oracles/apertus/Apertus.hh>

namespace oracles
{
  namespace apertus
  {
    Apertus::Apertus(reactor::Scheduler& sched,
                     std::string mhost, int mport,
                     std::string host, int port):
      _sched(sched),
      _meta(mhost, mport),
      _uuid(),
      _host(host),
      _port(port)
    {
      _meta.register_apertus(_uuid, port);
    }

    Apertus::~Apertus()
    {
      _meta.unregister_apertus(_uuid);
    }

    void
    Apertus::run()
    {
      reactor::network::TCPServer serv(_sched);
      serv.listen(_port);

      reactor::network::TCPSocket* client = nullptr;
      while ((client = serv.accept()) != nullptr)
      {
        // Retrieve TID size.
        char* buffer = new char[1];
        reactor::network::Buffer tmp(buffer, 1);
        client->read(tmp);
        int size = *buffer;
        delete[] buffer;

        // Retrieve TID of the client.
        buffer = new char[size];
        reactor::network::Buffer tid_buffer(buffer, size);
        client->read(tid_buffer);
        oracle::hermes::TID tid = std::string(buffer);
        delete[] buffer;

        // First client to connect with this TID, it must wait.
        if (_clients.find(tid) == _clients.end())
          _clients[tid] = client;
        // Second client, establishing connection.
        this->_connect(client, _clients[tid]);
      }
    }

    void
    Apertus::_connect(reactor::network::TCPSocket* client1,
                      reactor::network::TCPSocket* client2)
    {
      static const uint32_t buff_size = 1024 * 1024 * 16;

      auto continuous_read = [&] (reactor::network::TCPSocket* cl1,
                                  reactor::network::TCPSocket* cl2)
      {
        char* buff = new char[buff_size];
        reactor::network::Buffer recv(buff, buff_size);

        while (true)
        {
          uint32_t size = cl1->read_some(recv);
          elle::ConstWeakBuffer send(buff, size);
          cl2->write(send);
        }

        delete[] buff;
      };

      new reactor::Thread(_sched, "first", std::bind(continuous_read,
                                                     client1,
                                                     client2));
      new reactor::Thread(_sched, "second", std::bind(continuous_read,
                                                      client2,
                                                      client1));
    }
  }
}
