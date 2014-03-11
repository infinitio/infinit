#ifndef INFINIT_ORACLES_APERTUS_ACCEPTER_HH
# define INFINIT_ORACLES_APERTUS_ACCEPTER_HH

# include <infinit/oracles/apertus/fwd.hh>

# include <reactor/network/fwd.hh>
# include <reactor/thread.hh>
# include <reactor/timer.hh>

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
        typedef std::unique_ptr<reactor::network::Socket> Socket;

      public:
        /* We hold by-value a Thread and a Timer, which means a two-stage
        * destruction is required: stop all threads, and then asynchronously
        * delete the object (because stage 1 can be on one of the threads).
        * So use a custom unique_ptr deleter, that way users of this class
        * don't have to take care of that.
        */
        struct Deleter
        {
           void operator () (Accepter* p) const;
        };
        typedef std::unique_ptr<Accepter, Deleter> AccepterPtr;
        static AccepterPtr make(Apertus& apertus,
                 Socket&& client,
                 reactor::Duration timeout);
      private:
        friend struct Deleter;
        void destroy();
        Accepter(Apertus& apertus,
                 Socket&& client,
                 reactor::Duration timeout);
        virtual
        ~Accepter();
        void
        _handle();

        ELLE_ATTRIBUTE(Apertus&, apertus);
        ELLE_ATTRIBUTE(Socket, client);
        ELLE_ATTRIBUTE(reactor::Thread, accepter);
        ELLE_ATTRIBUTE(reactor::Timer, timeout);

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
