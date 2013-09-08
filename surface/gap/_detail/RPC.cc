# include <surface/gap/_detail/RPC.hh>

# include <surface/gap/Exception.hh>

# include <limits>
# include <fstream>
# include <ios>

ELLE_LOG_COMPONENT("surface.gap._detail.RPC");

namespace surface
{
  namespace gap
  {
    namespace _detail
    {
      elle::Buffer
      read(std::ifstream& file, uint64_t offset, uint64_t const size)
      {
        ELLE_TRACE_FUNCTION(file, offset, size);
        static const std::streamsize MAX_offset{
          std::numeric_limits<std::streamsize>::max()};
        static const size_t MAX_buffer{elle::Buffer::max_size};

        if (size > MAX_buffer)
          throw elle::Exception(
            elle::sprintf("buffer that big (%s) can't be addressed", size));

        if (!file.good())
          throw elle::Exception("file is broken");

        file.seekg(0, std::ios::beg);

        // If the offset is greater than the machine maximum streamsize, seekg n
        // times to reach the right offset.
        while (offset > MAX_offset)
        {
          file.seekg(MAX_offset, std::ios_base::cur);

          if (!file.good())
            throw elle::Exception(
              elle::sprintf("unable to increment offset by %s", MAX_offset));

          offset -= MAX_offset;
        }

        file.seekg(offset, std::ios_base::cur);

        if (!file.good())
          throw elle::Exception(
            elle::sprintf("unable to seek to pos %s", offset));

        elle::Buffer buffer(size);

        buffer.size(
          file.readsome(reinterpret_cast<char*>(buffer.mutable_contents()),
                        size));

        if (file.fail())
          throw elle::Exception("unable to read");

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
