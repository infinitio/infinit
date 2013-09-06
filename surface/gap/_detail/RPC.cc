#include <surface/gap/_detail/RPC.hh>

# include <surface/gap/Exception.hh>


# include <fstream>
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
           std::streamsize const size)
      {
        if (!file.good())
          throw elle::Exception("File is broken");

        file.seekg(pos, std::ios::beg);

        if (!file.good())
          throw elle::Exception("Unable to seek to pos");

        elle::Buffer buffer(size);

        buffer.size(
          file.readsome(reinterpret_cast<char*>(buffer.mutable_contents()), size));

        if (file.fail())
          throw elle::Exception("Unable to read");

        return buffer;
      }

      RPC::RPC(infinit::protocol::ChanneledStream& channels):
        infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                               elle::serialize::OutputBinaryArchive>(channels),
        _read("read", *this)
      {}
    }
  }
}
