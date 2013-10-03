#include <sys/stat.h>

#include <elle/Measure.hh>
#include <elle/concept/Fileable.hh>
#include <elle/finally.hh>
#include <elle/log.hh>
#include <elle/serialize/Serializable.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>

#include <hole/storage/Directory.hh>
#include <hole/Exception.hh>

#include <nucleus/factory.hh>
#include <nucleus/fwd.hh>

ELLE_LOG_COMPONENT("infinit.hole.storage.Directory");

namespace hole
{
  namespace storage
  {
    /*-------------.
    | Construction |
    `-------------*/

    Directory::Directory(nucleus::proton::Network const& network,
                         std::string const& root):
      Storage(network),
      _root(root)
    {}

    Directory::~Directory()
    {}

    /*----------.
    | Printable |
    `----------*/

    void
    Directory::print(std::ostream& stream) const
    {
      stream << "storage::Directory("
             << this->network() << ", "
             << this->root() << ")";
    }

    /*--------.
    | Storage |
    `--------*/

    bool
    Directory::_exist(std::string const& identifier) const
    {
      std::string path(this->path(identifier));

      ELLE_TRACE_SCOPE("%s: check if %s exists", *this, identifier);

      // XXX: duplicated from elle::io::File::Exist
      struct ::stat stat;
      if (::stat(path.c_str(), &stat) != 0)
        {
          return false;
        }
      if (!S_ISREG(stat.st_mode))
        return false;
      return true;
    }

    void
    Directory::_store(const nucleus::proton::Address& address,
                      const nucleus::proton::ImmutableBlock& block)
    {
      boost::filesystem::path path(this->path(address));
      boost::filesystem::create_directories(path.parent_path());

      ELLE_MEASURE("Serializing the block")
        elle::serialize::to_file(path.string()) << block;
    }

    void
    Directory::_store(const nucleus::proton::Address& address,
                      const nucleus::proton::MutableBlock& block)

    {
      boost::filesystem::path path(this->path(address));
      boost::filesystem::create_directories(path.parent_path());

      // Serialize the block.
      elle::serialize::to_file(path.string()) << block;
    }

    std::unique_ptr<nucleus::proton::Block>
    Directory::_load(nucleus::proton::Address const& address) const
    {
      // Get block path.
      elle::io::Path path(this->path(address));

      // Create an empty block.
      std::unique_ptr<nucleus::proton::ImmutableBlock> block{
        nucleus::factory::block().allocate<nucleus::proton::ImmutableBlock>(
          address.component())};

      // Deserialize the block.
      elle::serialize::from_file(path.string()) >> *block;

      return std::move(block);
    }

    std::unique_ptr<nucleus::proton::Block>
    Directory::_load(nucleus::proton::Address const& address,
                     nucleus::proton::Revision const& revision) const
    {
      // Get block path.
      elle::io::Path path(this->path(address, revision));

      // Create an empty block.
      std::unique_ptr<nucleus::proton::MutableBlock> block{
        nucleus::factory::block().allocate<nucleus::proton::MutableBlock>(
          address.component())};

      // Deserialize the block.
      elle::serialize::from_file(path.string()) >> *block;

      return std::move(block);
    }

    void
    Directory::_erase(nucleus::proton::Address const& address)
    {
      boost::filesystem::remove(boost::filesystem::path{this->path(address)});
    }

    /*----------.
    | Utilities |
    `----------*/
    std::string
    Directory::path(nucleus::proton::Address const& address,
                    nucleus::proton::Revision const& revision) const
    {
      std::string id(this->_identifier(address, revision));
      return path(id);
    }

    std::string
    Directory::path(std::string const& identifier) const
    {
      return this->_root + "/" + identifier + ".blk";
    }
  }
}
