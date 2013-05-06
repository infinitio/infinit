#include <hole/implementations/slug/Manifest.hh>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      RPC::RPC(infinit::protocol::ChanneledStream& channels):
        infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                               elle::serialize::OutputBinaryArchive>(channels),
        authenticate("authenticate", *this),
        push("push", *this),
        pull("pull", *this),
        wipe("wipe", *this)
      {}

      namespace control
      {
        RPC::RPC(infinit::protocol::ChanneledStream& channels):
          infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                                 elle::serialize::OutputBinaryArchive>(channels),
          slug_connect("slug_connect", *this),
          slug_wait("slug_wait", *this)
        {}
      }
    }
  }
}
