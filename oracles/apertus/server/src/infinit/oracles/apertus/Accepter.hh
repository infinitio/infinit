#ifndef INFINIT_ORACLES_APERTUS_ACCEPTER_HH
# define INFINIT_ORACLES_APERTUS_ACCEPTER_HH

# include <infinit/oracles/apertus/fwd.hh>

# include <reactor/network/fwd.hh>
# include <reactor/thread.hh>

# include <elle/attribute.hh>

# include <memory>
namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      class Accepter
      {
        typedef std::unique_ptr<reactor::network::TCPSocket> Socket;
      public:
        Accepter(Apertus& apertus,
                 Socket&& client);

        ~Accepter();

      private:
        void
        _handle();

        ELLE_ATTRIBUTE(Apertus&, apertus);
        ELLE_ATTRIBUTE(Socket, client);
        ELLE_ATTRIBUTE(reactor::Thread, accepter);
      };
    }
  }
}

#endif
