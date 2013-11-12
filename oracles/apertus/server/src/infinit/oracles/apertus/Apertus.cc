#include <infinit/oracles/apertus/Apertus.hh>
//TODO: unique_ptr autoptr

namespace oracles
{
  namespace apertus
  {
    Apertus::Apertus(reactor::Scheduler& sched,
                     std::string mhost, int mport,
                     std::string host, int port):
      _sched(sched),
      _meta(nullptr),
      _uuid(),
      _host(host),
      _port(port)
    {
      // Allow the server not to connect to meta if using meta port 0.
      // Useful for tests purposes.
      if (mport != 0)
      {
        _meta = new infinit::oracles::meta::Admin(mhost, mport);
        _meta->register_apertus(_uuid, port);
      }
    }

    Apertus::~Apertus()
    {
      // Allow the server not to connect to meta if using meta port 0.
      // Useful for tests purposes.
      if (_meta != nullptr)
      {
        _meta->unregister_apertus(_uuid);
        delete _meta;
      }
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
        char size;
        reactor::network::Buffer tmp(&size, 1);
        client->read(tmp);

        // Retrieve TID of the client.
        char tid_array[size];
        reactor::network::Buffer tid_buffer(tid_array, size);
        client->read(tid_buffer);
        oracle::hermes::TID tid = std::string(tid_array);

        // First client to connect with this TID, it must wait.
        if (_clients.find(tid) == _clients.end())
          _clients[tid] = client;
        // Second client, establishing connection.
        else
        {
          this->_connect(client, _clients[tid]);
          _clients.erase(tid);
        }
      }
    }

    std::map<oracle::hermes::TID, reactor::network::TCPSocket*>&
    Apertus::get_clients()
    {
      return _clients;
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

        try
        {
          while (true)
          {
            uint32_t size = cl1->read_some(recv);
            elle::ConstWeakBuffer send(buff, size);
            cl2->write(send);
          }
        }
        catch (std::runtime_error const&)
        {}

        delete[] buff;
      };

      new reactor::Thread(_sched, "left_to_right", std::bind(continuous_read,
                                                             client1,
                                                             client2));
      new reactor::Thread(_sched, "right_to_left", std::bind(continuous_read,
                                                             client2,
                                                             client1));
    }
  }
}
