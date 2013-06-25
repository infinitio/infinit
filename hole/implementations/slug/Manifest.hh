#ifndef HOLE_IMPLEMENTATIONS_SLUG_MANIFEST_HH
# define HOLE_IMPLEMENTATIONS_SLUG_MANIFEST_HH

# include <elle/network/Locus.hh>
# include <elle/serialize/BinaryArchive.hh>
# include <elle/fwd.hh>

# include <hole/fwd.hh>

# include <nucleus/fwd.hh>

# include <protocol/RPC.hh>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      struct RPC:
        public infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                                      elle::serialize::OutputBinaryArchive>
      {
        RemoteProcedure<std::vector<elle::network::Locus>,
                        hole::Passport const&> authenticate;
        RemoteProcedure<void, nucleus::proton::Address const&,
                        nucleus::Derivable&> push;
        RemoteProcedure<nucleus::Derivable,
                        nucleus::proton::Address const&,
                        nucleus::proton::Revision const&> pull;
        RemoteProcedure<void, nucleus::proton::Address const&> wipe;
        RPC(infinit::protocol::ChanneledStream& channels);
      };

      namespace control
      {
        struct RPC:
          public infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                                          elle::serialize::OutputBinaryArchive>
        {
          RPC(infinit::protocol::ChanneledStream& channels);

          RemoteProcedure<void, std::string const&, int>
          slug_connect;

          RemoteProcedure<bool, std::string const&, int>
          slug_wait;
        };
      }
    }
  }
}

#endif
