#include <boost/lexical_cast.hpp>

#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/resolve.hh>
#include <reactor/network/socket-operation.hh>
#include <reactor/network/udp-server.hh>
#include <reactor/network/udp-socket.hh>
#include <reactor/scheduler.hh>
#include <reactor/thread.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("reactor.network.UDPSocket");

namespace reactor
{
  namespace network
  {
    /*-------------.
    | Construction |
    `-------------*/

    UDPSocket::UDPSocket(Scheduler& sched,
                         const std::string& hostname,
                         const std::string& port)
      : Super(sched, resolve_udp(sched, hostname, port))
    {}

    UDPSocket::UDPSocket(Scheduler& sched,
                         const std::string& hostname,
                         int port)
      : Super(sched, resolve_udp(sched, hostname,
                                 boost::lexical_cast<std::string>(port)))
    {}

    UDPSocket::UDPSocket(Scheduler& sched,
                         int local_port,
                         const std::string& hostname,
                         int port)
      : Super(sched, new AsioSocket(sched.io_service()))
    {
      this->_socket->open(boost::asio::ip::udp::v4());
      boost::asio::ip::udp::endpoint local(boost::asio::ip::udp::v4(),
                                           local_port);
      this->_socket->bind(local);
      this->_connect(resolve_udp(sched, hostname,
                                 boost::lexical_cast<std::string>(port)));
    }

    UDPSocket::UDPSocket(Scheduler& sched)
      : Super(sched, new boost::asio::ip::udp::socket(sched.io_service()))
    {}

    UDPSocket::~UDPSocket()
    {}

    /*--------------.
    | Configuration |
    `--------------*/

    void
    UDPSocket::bind(boost::asio::ip::udp::endpoint const& endpoint)
    {
      socket()->open(boost::asio::ip::udp::v4());
      socket()->bind(endpoint);
    }

    /*-----------.
    | Connection |
    `-----------*/

    void
    UDPSocket::connect(const std::string& hostname, int service)
    {
      connect(hostname, boost::lexical_cast<std::string>(service));
    }

    void
    UDPSocket::connect(const std::string& hostname,
                       const std::string& service)
    {
      _socket = new boost::asio::ip::udp::socket(scheduler().io_service());
      _socket->connect(resolve_udp(scheduler(), hostname, service));
      //Super::connect(resolve_udp(scheduler(), hostname, service));
    }

    /*-----.
    | Read |
    `-----*/

    class UDPRead: public SocketOperation<boost::asio::ip::udp::socket>
    {
      public:
        typedef boost::asio::ip::udp::socket AsioSocket;
        typedef boost::asio::ip::udp::endpoint EndPoint;
        typedef SocketOperation<AsioSocket> Super;
        UDPRead(Scheduler& scheduler,
             PlainSocket<AsioSocket>* socket,
             Buffer& buffer)
          : Super(scheduler, socket)
          , _buffer(buffer)
          , _read(0)
        {}

        virtual const char* type_name() const
        {
          static const char* name = "socket read";
          return name;
        }

        Size
        read()
        {
          return _read;
        }

      protected:

        virtual void _start()
        {
          // FIXME: be synchronous if enough bytes are available
          EndPoint peer;
          this->socket()->async_receive(
            boost::asio::buffer(_buffer.data(), _buffer.size()),
            boost::bind(&UDPRead::_wakeup, this, _1, _2));
        }

      private:
        void _wakeup(const boost::system::error_code& error,
                     std::size_t read)
        {
          if (error == boost::system::errc::operation_canceled)
            return;
          _read = read;
          if (error == boost::asio::error::eof)
            this->_raise(new ConnectionClosed());
          else if (error)
            this->_raise(new Exception(error.message()));
          this->_signal();
        }

        Buffer& _buffer;
        Size _read;
    };

    Size
    UDPSocket::read_some(Buffer buffer, DurationOpt timeout)
    {
      if (timeout)
        ELLE_TRACE("%s: read at most %s bytes with timeout %s",
                       *this, buffer.size(), timeout);
      else
        ELLE_TRACE("%s: read at most %s bytes",
                       *this, buffer.size());
      UDPRead read(scheduler(), this, buffer);
      if (!read.run(timeout))
        throw TimeOut();
      return read.read();
    }


    class UDPRecvFrom: public SocketOperation<boost::asio::ip::udp::socket>
    {
      public:
        typedef boost::asio::ip::udp::socket AsioSocket;
        typedef boost::asio::ip::udp::endpoint EndPoint;
        typedef SocketOperation<AsioSocket> Super;
        UDPRecvFrom(Scheduler& scheduler,
                    PlainSocket<AsioSocket>* socket,
                    Buffer& buffer,
                    boost::asio::ip::udp::endpoint & endpoint)
          : Super(scheduler, socket)
          , _buffer(buffer)
          , _read(0)
          , _endpoint(endpoint)
        {}

        virtual const char* type_name() const
        {
          static const char* name = "socket recv_from";
          return name;
        }

        Size
        read()
        {
          return _read;
        }

      protected:

        virtual void _start()
        {
          auto wake = [&] (boost::system::error_code const e, std::size_t w) {
            this->_wakeup(e, w);
          };

          this->socket()->async_receive_from(
            boost::asio::buffer(_buffer.data(), _buffer.size()),
            this->_endpoint,
            wake);
        }

      private:
        void _wakeup(const boost::system::error_code& error,
                     std::size_t read)
        {
          if (error == boost::system::errc::operation_canceled)
            return;
          _read = read;
          if (error == boost::asio::error::eof)
            this->_raise(new ConnectionClosed());
          else if (error)
            this->_raise(new Exception(error.message()));
          this->_signal();
        }

        Buffer& _buffer;
        Size _read;
        boost::asio::ip::udp::endpoint &_endpoint;
    };

    Size
    UDPSocket::receive_from(Buffer buffer,
                            boost::asio::ip::udp::endpoint &endpoint,
                            DurationOpt timeout)
    {
      if (timeout)
        ELLE_TRACE("%s: read at most %s bytes with timeout %s",
                       *this, buffer.size(), timeout);
      else
        ELLE_TRACE("%s: read at most %s bytes",
                       *this, buffer.size());
      UDPRecvFrom recvfrom(scheduler(), this, buffer, endpoint);
      if (!recvfrom.run(timeout))
        throw TimeOut();
      return recvfrom.read();
    }


    /*------.
    | Write |
    `------*/

    class UDPWrite: public SocketOperation<boost::asio::ip::udp::socket>
    {
      public:
        typedef boost::asio::ip::udp::socket AsioSocket;
        typedef boost::asio::ip::udp::endpoint EndPoint;
        typedef SocketOperation<AsioSocket> Super;
        UDPWrite(Scheduler& scheduler,
              PlainSocket<AsioSocket>* socket,
              Buffer& buffer)
          : Super(scheduler, socket)
          , _buffer(buffer)
          , _written(0)
        {}

      protected:
        virtual void _start()
        {
          this->socket()->async_send(boost::asio::buffer(_buffer.data(),
                                                         _buffer.size()),
                                     boost::bind(&UDPWrite::_wakeup,
                                                 this, _1, _2));
        }

      private:
        void _wakeup(const boost::system::error_code& error,
                     std::size_t written)
        {
          _written = written;
          if (error == boost::asio::error::eof)
            this->_raise(new ConnectionClosed());
          else if (error)
            this->_raise(new Exception(error.message()));
          this->_signal();
        }

        Buffer& _buffer;
        Size _written;
    };

    class UDPSendTo: public SocketOperation<boost::asio::ip::udp::socket>
    {
      public:
        typedef boost::asio::ip::udp::socket AsioSocket;
        typedef boost::asio::ip::udp::endpoint EndPoint;
        typedef SocketOperation<AsioSocket> Super;
        UDPSendTo(Scheduler& scheduler,
                  PlainSocket<AsioSocket>* socket,
                  Buffer& buffer,
                  EndPoint const & endpoint)
          : Super(scheduler, socket)
          , _buffer(buffer)
          , _written(0)
          , _endpoint(endpoint)
        {}

      protected:
        virtual void _start()
        {
          auto wake = [&] (boost::system::error_code const e, std::size_t w) {
            this->_wakeup(e, w);
          };

          this->socket()->async_send_to(boost::asio::buffer(_buffer.data(),
                                                         _buffer.size()),
                                        this->_endpoint,
                                        wake);
        }

      private:
        void _wakeup(const boost::system::error_code& error,
                     std::size_t written)
        {
          _written = written;
          if (error == boost::asio::error::eof)
            this->_raise(new ConnectionClosed());
          else if (error)
            this->_raise(new Exception(error.message()));
          this->_signal();
        }

        Buffer& _buffer;
        Size _written;
        EndPoint _endpoint;
    };

    void
    UDPSocket::write(Buffer buffer)
    {
      ELLE_TRACE("%s: write %s bytes to %s",
                     *this, buffer.size(), _socket->remote_endpoint());
      UDPWrite write(scheduler(), this, buffer);
      write.run();
    }

    void
    UDPSocket::send_to(Buffer buffer,
                       EndPoint endpoint)
    {
      ELLE_TRACE("%s: send_to %s bytes to %s",
                     *this, buffer.size(), endpoint);
      UDPSendTo sendto(scheduler(), this, buffer, endpoint);
      sendto.run();
    }

    /*----------------.
    | Pretty Printing |
    `----------------*/

    void
    UDPSocket::print(std::ostream& s) const
    {
      s << "UDP Socket " << _socket->local_endpoint();
    }
  }
}
