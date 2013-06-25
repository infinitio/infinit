#ifndef HOLE_IMPLEMENTATIONS_SLUG_HOST_HH
# define HOLE_IMPLEMENTATIONS_SLUG_HOST_HH

# include <reactor/network/socket.hh>

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/network/fwd.hh>
# include <elle/network/Locus.hh>

# include <protocol/ChanneledStream.hh>
# include <protocol/Serializer.hh>

# include <hole/implementations/slug/Slug.hh>
# include <hole/implementations/slug/Manifest.hh>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      class Host
      {
      public:
        // Enumerations
        enum class State
        {
          connected,
          authenticating,
          authenticated,
          dead,
        };

      /*-------------.
      | Construction |
      `-------------*/
      public:
        Host(Slug& slug,
             elle::network::Locus const& locus,
             std::unique_ptr<reactor::network::Socket> socket);
        ~Host();

      /*-----------.
      | Attributes |
      `-----------*/
      private:
        Slug& _slug;
        /// The remote locus.
        ELLE_ATTRIBUTE_R(elle::network::Locus, locus);
        /// Our own state.
        ELLE_ATTRIBUTE_R(State, state);
        /// Whether the remote host is authenticated.
        ELLE_ATTRIBUTE_R(elle::Boolean, authenticated);
        std::unique_ptr<Passport> _remote_passport;
        std::unique_ptr<reactor::network::Socket> _socket;
        infinit::protocol::Serializer _serializer;
        infinit::protocol::ChanneledStream _channels;

      public:
        inline
        Passport const&
        remote_passport() const;

        inline
        void
        remote_passport(Passport const&);
      /*----.
      | RPC |
      `----*/
      private:
        void _rpc_run();
        RPC _rpcs;
        reactor::Thread* _rpcs_handler;

      /*----.
      | API |
      `----*/
      public:
        std::vector<elle::network::Locus>
        authenticate(Passport const& passport);
        std::unique_ptr<nucleus::proton::Block>
        pull(nucleus::proton::Address const& address,
             nucleus::proton::Revision const& revision);
        void
        push(nucleus::proton::Address const& address,
             nucleus::proton::Block const& block);
        void
        wipe(nucleus::proton::Address const& address);
      private:
        std::vector<elle::network::Locus>
        _authenticate(Passport const& passport);
        void
        _push(nucleus::proton::Address const& address,
              nucleus::Derivable& derivable);
        nucleus::Derivable
        _pull(nucleus::proton::Address const&,
              nucleus::proton::Revision const&);
        void
        _wipe(nucleus::proton::Address const&);

      /*---------.
      | Dumpable |
      `---------*/
      public:
        elle::Status    Dump(const elle::Natural32 = 0) const;

      /*-------------.
      | Pretty print |
      `-------------*/
      public:
        void print(std::ostream& stream) const;
      };

      std::ostream&
      operator << (std::ostream& stream, const Host& host);
    }
  }
}

#include <hole/implementations/slug/Host.hxx>

#endif
