#ifndef INFINIT_ORACLES_APERTUS_TRANSFER_HH
# define INFINIT_ORACLES_APERTUS_TRANSFER_HH

# include <infinit/oracles/apertus/Apertus.hh>

# include <reactor/network/fwd.hh>
# include <reactor/thread.hh>
# include <reactor/waitable.hh>

# include <elle/Printable.hh>
# include <elle/attribute.hh>

# include <memory>

namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      class Transfer:
        public reactor::Waitable
      {
        typedef std::unique_ptr<reactor::network::Socket> Socket;
      public:
        Transfer(Apertus& owner,
                 oracle::hermes::TID tid,
                 Socket&& left,
                 Socket&& right);
        virtual
        ~Transfer();
        Transfer(Transfer const&) = delete;
        Transfer(Transfer&&) = delete;

      private:
        void
        _run();

      private:
        void
        _handle(Socket&, Socket&);

        ELLE_ATTRIBUTE(Apertus&, apertus);
        ELLE_ATTRIBUTE_R(oracle::hermes::TID, tid);
        ELLE_ATTRIBUTE(Socket, left);
        ELLE_ATTRIBUTE(Socket, right);
        ELLE_ATTRIBUTE(reactor::Thread, forward);

        /*----------.
        | Printable |
        `----------*/
        virtual
        void
        print(std::ostream& stream) const;
      };
    }
  }
}

#endif
