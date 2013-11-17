#ifndef INFINIT_ORACLES_APERTUS_ACCEPTER_HH
# define INFINIT_ORACLES_APERTUS_ACCEPTER_HH

# include <infinit/oracles/apertus/fwd.hh>

# include <reactor/network/fwd.hh>
# include <reactor/thread.hh>

# include <elle/attribute.hh>
# include <elle/Printable.hh>

# include <memory>
namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      class Accepter:
        public elle::Printable
      {
        typedef std::unique_ptr<reactor::network::TCPSocket> Socket;
      public:
        Accepter(Apertus& apertus,
                 Socket&& client);

        virtual
        ~Accepter();

      private:
        void
        _handle();

        ELLE_ATTRIBUTE(Apertus&, apertus);
        ELLE_ATTRIBUTE(Socket, client);
        ELLE_ATTRIBUTE(reactor::Thread, accepter);

        /*----------.
        | Printable |
        `----------*/
        void
        print(std::ostream& stream) const;
      };
    }
  }
}

#endif
