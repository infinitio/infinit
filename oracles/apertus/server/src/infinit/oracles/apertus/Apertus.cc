#include <infinit/oracles/apertus/Apertus.hh>
//TODO: unique_ptr autoptr

namespace oracles
{
  namespace apertus
  {
    Apertus::Apertus(reactor::Scheduler& sched,
                     std::string mhost, int mport,
                     std::string host, int port):
      Waitable("apertus"),
      _sched(sched),
      _accepter(sched,
                "apertus_accepter",
                std::bind(&Apertus::_run, std::ref(*this))),
      _meta(nullptr),
      _uuid(boost::uuids::random_generator()()),
      _host(host),
      _port(port)
    {
      if (mport != 0)
        _meta = new infinit::oracles::meta::Admin(mhost, mport);
    }

    Apertus::~Apertus()
    {
      _accepter.terminate_now();

      for (auto worker(_workers.begin()); worker != _workers.end(); worker++)
      {
        (*worker)->terminate_now();
        worker->reset();
      }

      if (_meta != nullptr)
        delete _meta;
    }

    void
    Apertus::reg()
    {
      if (_meta != nullptr)
        _meta->register_apertus(_uuid, _port);
    }

    void
    Apertus::unreg()
    {
      if (_meta != nullptr)
        _meta->unregister_apertus(_uuid);
    }

    void
    Apertus::_run()
    {
      reactor::network::TCPServer serv(_sched);
      serv.listen(_port);

      this->reg();

      elle::With<elle::Finally>([this] { this->unreg(); }) << [&]
      {
        auto handle = [&] (reactor::network::TCPSocket* client)
        {
          std::cout << "ENTER HANDLE" << std::endl;

          // Retrieve TID size.
          char size;
          reactor::network::Buffer tmp(&size, 1);
          client->read(tmp);

          std::cout << "GOT SIZE: " << (int)size << std::endl;

          // Retrieve TID of the client.
          char tid_array[size + 1]; tid_array[size] = '\0';
          reactor::network::Buffer tid_buffer(tid_array, size);
          client->read(tid_buffer);
          oracle::hermes::TID tid = std::string(tid_array);

          std::cout << "GOT TID: " << tid << std::endl;

          // First client to connect with this TID, it must wait.
          if (_clients.find(tid) == _clients.end())
          {
            std::cout << "ADD TO MAP: " << client << std::endl;
            _clients[tid] = client;
          }
          // Second client, establishing connection.
          else
          {
            std::cout << "CONNECT: " << client << " " << _clients[tid] << std::endl;
            this->_connect(client, _clients[tid]);
            _clients.erase(tid);
          }
        };

        reactor::network::TCPSocket* client = nullptr;
        while ((client = serv.accept()) != nullptr)
        {
          std::cout << "GOT CLIENT" << std::endl;
          new reactor::Thread(_sched, "handler", std::bind(handle, client));
        }
      };
    }

    void
    Apertus::stop()
    {
      this->_signal();
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

      std::shared_ptr<reactor::network::TCPSocket> cl1(client1);
      std::shared_ptr<reactor::network::TCPSocket> cl2(client2);

      std::cout << "Before destruction?" << cl1.use_count() << std::endl;

      auto continuous_read = [buff_size] (
        std::shared_ptr<reactor::network::TCPSocket> cli1,
        std::shared_ptr<reactor::network::TCPSocket> cli2)
      {
        char buff [buff_size];

      std::cout << "After destruction?" << cli1.use_count() << std::endl;

        reactor::network::Buffer recv(buff, buff_size);

        try
        {
          while (true)
          {
            uint32_t size = cli1->read_some(recv);
            elle::ConstWeakBuffer send(buff, size);
            cli2->write(send);
          }
        }
        catch (std::runtime_error const&)
        {}
      };

      _workers.emplace_back(new reactor::Thread(
          _sched, "left_to_right",
          [cl1, cl2, continuous_read] { continuous_read(cl1, cl2); }));

      _workers.emplace_back(new reactor::Thread(
          _sched, "right_to_left",
          [cl1, cl2, continuous_read] { continuous_read(cl2, cl1); }));

      std::cout << "fin du scope" << std::endl;
    }
  }
}
