#include <hole/storage/Memory.hh>
#include <hole/Exception.hh>

#include <elle/log.hh>
#include <elle/finally.hh>
#include <elle/Exception.hh>
#include <elle/serialize/Serializable.hh>
#include <elle/serialize/insert.hh>
#include <elle/serialize/extract.hh>
#include <elle/io/Unique.hh>

#include <nucleus/fwd.hh>
#include <nucleus/Derivable.hh>

#include <boost/format.hpp>

#include <sstream>

ELLE_LOG_COMPONENT("infinit.hole.storage.Memory");

namespace hole
{
  namespace storage
  {
    /*-------------.
    | Construction |
    `-------------*/

    Memory::Memory(nucleus::proton::Network const& network):
      Storage(network)
    {}

    Memory::~Memory()
    {
      this->_container.clear();
    }

    /*--------.
    | Methods |
    `--------*/

    elle::Boolean
    Memory::empty() const
    {
      return (this->_container.empty());
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Memory::dump() const
    {
      std::cout << "[Memory] #" << this->_container.size() << std::endl;

      for (auto& pair: this->_container)
        {
          std::cout << "  " << pair.first << std::endl;
        }
    }

    void
    Memory::print(std::ostream& stream) const
    {
      stream << "storage::Memory(" << this->network() << ")";
    }

    /*--------.
    | Storage |
    `--------*/

    bool
    Memory::_exist(std::string const& identifier) const
    {
      ELLE_TRACE_METHOD(identifier);

      return (this->_container.find(identifier) != this->_container.end());
    }

    void
    Memory::_store(const nucleus::proton::Address& address,
                   const nucleus::proton::ImmutableBlock& block)
    {
      ELLE_TRACE_METHOD(address, block);

      // Convert the address and block into strings.
      elle::io::Unique unique_address{address.unique()};

      // Serialize the block.
      nucleus::Derivable derivable(address.component(), block);
      elle::io::Unique value;

      elle::serialize::to_string(value) << derivable;

      // Insert in the container.
      auto result =
        this->_container.insert(
          std::pair<elle::String const, elle::String const>(unique_address,
                                                            value));

      // Check if the insertion was successful.
      if (result.second == false)
        throw Exception("unable to insert the pair address/block "
                              "in the container");
    }

    void
    Memory::_store(const nucleus::proton::Address& address,
                   const nucleus::proton::MutableBlock& block)
    {
      ELLE_TRACE_METHOD(address, block);

      // Convert the address and block into strings.
      elle::io::Unique unique_address{address.unique()};

      // Serialize the block.
      nucleus::Derivable derivable(address.component(), block);
      std::string value;

      elle::serialize::to_string(value) << derivable;

      // Insert in the container.
      this->_container.erase(unique_address);
      this->_container.insert(
        std::pair<elle::String const, elle::String const>(unique_address,
                                                          value));
    }

    std::unique_ptr<nucleus::proton::Block>
    Memory::_load(nucleus::proton::Address const& address) const
    {
      ELLE_TRACE_METHOD(address);

      // Convert the address to a string.
      elle::io::Unique unique_address{address.unique()};

      ELLE_ASSERT(this->_exist(unique_address) == true);

      nucleus::Derivable derivable;

      elle::serialize::from_string(this->_container.at(unique_address)) >>
        derivable;

      ELLE_ASSERT_EQ(derivable.block().bind(), address);

      return derivable.release();
    }

    std::unique_ptr<nucleus::proton::Block>
    Memory::_load(nucleus::proton::Address const& address,
                  nucleus::proton::Revision const& revision) const
    {
      ELLE_TRACE_METHOD(address, revision);

      // Since the storage does not handle revisions.
      return (this->_load(address));
    }

    void
    Memory::_erase(nucleus::proton::Address const& address)
    {
      ELLE_TRACE_METHOD(address);

      // Convert the address to a string.
      elle::io::Unique unique_address{address.unique()};

      ELLE_ASSERT(this->_exist(unique_address) == true);

      this->_container.erase(unique_address);
    }
  }
}
