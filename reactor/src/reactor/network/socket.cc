#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/socket.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/network/udt-socket.hh>
#include <reactor/network/socket-operation.hh>
#include <reactor/scheduler.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("reactor.network.Socket");

namespace reactor
{
  namespace network
  {
    namespace
    {
      class StreamBuffer: public elle::DynamicStreamBuffer
      {
      public:
        typedef elle::DynamicStreamBuffer Super;
        typedef Super::Size Size;
        StreamBuffer(Socket* socket)
        : Super(1 << 16)
        , _socket(socket)
        {}

      protected:
        virtual Size read(char* buffer, Size size)
        {
          return _socket->read_some(network::Buffer(buffer, size));
        }

        virtual void write(char* buffer, Size size)
        {
          _socket->write(network::Buffer(buffer, size));
        }


      private:
        Socket* _socket;
      };
    }

    /*-------------.
    | Construction |
    `-------------*/

    Socket::Socket(Scheduler& sched)
      : elle::IOStream(new StreamBuffer(this))
      , _sched(sched)
    {}

    Socket::~Socket()
    {}

    std::unique_ptr<Socket>
    Socket::create(Protocol protocol,
                   Scheduler& sched,
                   const std::string& hostname,
                   int port,
                   DurationOpt timeout)
    {
      switch (protocol)
        {
          case Protocol::tcp:
            return std::unique_ptr<Socket>
              (new TCPSocket(sched, hostname, port, timeout));
          case Protocol::udt:
            return std::unique_ptr<Socket>
              (new UDTSocket(sched, hostname, port, timeout));
          default:
            elle::unreachable();
        }
    }

    /*----------------.
    | Pretty printing |
    `----------------*/

    template <typename AsioSocket>
    void
    PlainSocket<AsioSocket>::print(std::ostream& s) const
    {
      s << "reactor::network::Socket(" << this->peer() << ")";
    }

    std::ostream& operator << (std::ostream& s, const Socket& socket)
    {
      socket.print(s);
      return s;
    }

    /*-------------.
    | Construction |
    `-------------*/

    template <typename AsioSocket>
    PlainSocket<AsioSocket>::PlainSocket(Scheduler& sched,
                                         const EndPoint& peer,
                                         DurationOpt timeout)
      : Super(sched)
      , _socket(0)
      , _peer(peer)
    {
      _connect(_peer, timeout);
    }

    template <typename AsioSocket>
    PlainSocket<AsioSocket>::PlainSocket(Scheduler& sched,
                                         AsioSocket* socket)
      : Super(sched)
      , _socket(socket)
    {}

    template <typename AsioSocket>
    PlainSocket<AsioSocket>::~PlainSocket()
    {
      _disconnect();
      delete _socket;
    }

    /*-----------.
    | Connection |
    `-----------*/

    template <typename AsioSocket>
    class Connection: public SocketOperation<AsioSocket>
    {
      public:
        typedef typename AsioSocket::endpoint_type EndPoint;
        typedef SocketOperation<AsioSocket> Super;
        Connection(Scheduler& scheduler,
                   PlainSocket<AsioSocket>* socket,
                   const EndPoint& endpoint)
          : Super(scheduler, socket)
          , _endpoint(endpoint)
        {}

        virtual const char* type_name() const
        {
          static const char* name = "socket connect";
          return name;
        }

      protected:
        virtual void _start()
        {
          this->socket()->async_connect(
            _endpoint, boost::bind(&Connection::_wakeup, this, _1));
        }

      private:
        EndPoint _endpoint;
    };

    template <typename AsioSocket>
    void
    PlainSocket<AsioSocket>::_connect(const EndPoint& endpoint, DurationOpt timeout)
    {
      ELLE_TRACE("%s: connecting to %s", *this, endpoint);
      if (!this->_socket)
        this->_socket = new AsioSocket(this->scheduler().io_service());
      Connection<AsioSocket> connection(this->scheduler(), this, endpoint);
      try
      {
        if (!connection.run(timeout))
          throw TimeOut();
      }
      catch (...)
      {
        delete _socket;
        _socket = 0;
        throw;
      }
    }

    template <typename AsioSocket>
    void
    PlainSocket<AsioSocket>::_disconnect()
    {
      if (_socket)
      {
        boost::system::error_code error;
        _socket->shutdown(AsioSocket::shutdown_both, error);
        if (error)
          {
            if (error == boost::asio::error::not_connected)
              ; // It's ok to try to disconnect a non-connected socket.
            else
              throw new Exception(error.message());
          }
        _socket->close();
        delete _socket;
        _socket = 0;
      }
    }

    /*-----------.
    | Scheduling |
    `-----------*/

    Scheduler&
    Socket::scheduler()
    {
      return _sched;
    }

    /*-----.
    | Read |
    `-----*/

    void
    Socket::read(network::Buffer, DurationOpt)
    {
      std::abort();
      // XXX[unused arguments for now, do something with it]
      // Size s = buffer.size();
      // read_some(buffer);
    }

    /*------.
    | Write |
    `------*/

    void
    Socket::write(const char* data)
    {
      network::Buffer buffer(data, strlen(data));
      return write(buffer);
    }

    /*-----------.
    | Properties |
    `-----------*/


    template <typename AsioSocket>
    static elle::network::Locus
    locus_from_endpoint(typename AsioSocket::endpoint_type const& endpoint)
    {
      auto host = boost::lexical_cast<std::string>(endpoint.address());
      auto port = endpoint.port();
      return elle::network::Locus(host, port);
    }

    template <typename AsioSocket>
    elle::network::Locus
    PlainSocket<AsioSocket>::local_locus() const
    {
      return locus_from_endpoint<AsioSocket>(local_endpoint());
    }

    template <typename AsioSocket>
    elle::network::Locus
    PlainSocket<AsioSocket>::remote_locus() const
    {
      return locus_from_endpoint<AsioSocket>(peer());
    }

    template <typename AsioSocket>
    typename PlainSocket<AsioSocket>::EndPoint
    PlainSocket<AsioSocket>::peer() const
    {
      return this->_peer;
    }

    template <typename AsioSocket>
    typename PlainSocket<AsioSocket>::EndPoint
    PlainSocket<AsioSocket>::local_endpoint() const
    {
        return this->_socket->local_endpoint();
    }

    /*------------------------.
    | Explicit instantiations |
    `------------------------*/

    template
    class PlainSocket<boost::asio::ip::tcp::socket>;
    template
    class PlainSocket<boost::asio::ip::udp::socket>;
    template
    class PlainSocket<boost::asio::ip::udt::socket>;
  }
}
