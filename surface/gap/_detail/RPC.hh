#ifndef RPC_HH
# define RPC_HH

#include <protocol/ChanneledStream.hh>
#include <protocol/Serializer.hh>
#include <protocol/RPC.hh>

# include <elle/serialize/BinaryArchive.hh>
# include <elle/Buffer.hh>

# include <ios>

namespace surface
{
  namespace gap
  {
    namespace _detail
    {
      elle::Buffer
      read(std::ifstream& file,
           std::streamsize const pos,
           std::streamsize const size);

      struct RPC:
        public infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                                      elle::serialize::OutputBinaryArchive>
      {
        RPC(infinit::protocol::ChanneledStream& channels);
        RemoteProcedure<elle::Buffer,
                        uint32_t,
                        std::streamsize,
                        std::streamsize> _read;
      };
    }
  }
}





#endif
