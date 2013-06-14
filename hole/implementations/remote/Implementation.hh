#ifndef HOLE_IMPLEMENTATIONS_REMOTE_IMPLEMENTATION_HH
# define HOLE_IMPLEMENTATIONS_REMOTE_IMPLEMENTATION_HH

# include <elle/fwd.hh>
# include <elle/network/Locus.hh>
# include <elle/types.hh>

# include <nucleus/proton/fwd.hh>

# include <hole/Hole.hh>

namespace hole
{
  namespace implementations
  {
    namespace remote
    {
      /// Remote hole implementation.
      class Implementation: public Hole
      {

      /*-------------.
      | Construction |
      `-------------*/
      public:
        Implementation(hole::storage::Storage& storage,
                       elle::Passport const& passport,
                       elle::Authority const& authority,
                       elle::network::Locus const& server);
        ~Implementation();
        ELLE_ATTRIBUTE_R(elle::network::Locus, server_locus);

      /*-----.
      | Hole |
      `-----*/
      protected:
        virtual
        void
        _push(const nucleus::proton::Address& address,
              const nucleus::proton::ImmutableBlock& block);
        virtual
        void
        _push(const nucleus::proton::Address& address,
              const nucleus::proton::MutableBlock& block);
        virtual
        std::unique_ptr<nucleus::proton::Block>
        _pull(const nucleus::proton::Address&);
        virtual
        std::unique_ptr<nucleus::proton::Block>
        _pull(const nucleus::proton::Address&, const nucleus::proton::Revision&);
        virtual
        void
        _wipe(const nucleus::proton::Address& address);

      /*---------.
      | Dumpable |
      `---------*/
      public:
        elle::Status            Dump(const elle::Natural32 = 0) const;
      };

    }
  }
}

#endif
