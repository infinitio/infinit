#ifndef INFINIT_REACTOR_NETWORK_EXCEPTION_HH
# define INFINIT_REACTOR_NETWORK_EXCEPTION_HH

# include <reactor/exception.hh>

namespace reactor
{
  namespace network
  {
    class Exception: public reactor::Exception
    {
    public:
      typedef reactor::Exception Super;
      Exception(const std::string& message);
      ELLE_EXCEPTION(Exception);
    };

    class ConnectionClosed: public Exception
    {
    public:
      typedef Exception Super;
      ConnectionClosed();
      ELLE_EXCEPTION(ConnectionClosed);
    };

    class TimeOut: public Exception
    {
    public:
      typedef Exception Super;
      TimeOut();
      ELLE_EXCEPTION(TimeOut);
    };
  }
}

#endif
