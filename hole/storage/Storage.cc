#include <elle/cast.hh>
#include <elle/assert.hh>

#include <boost/format.hpp>

#include <hole/storage/Storage.hh>
#include <hole/Exception.hh>

ELLE_LOG_COMPONENT("infinit.hole.storage.Storage");

namespace hole
{
  namespace storage
  {
    /*-------------.
    | Construction |
    `-------------*/

    Storage::Storage(nucleus::proton::Network const& network):
      _network(network)
    {}

    Storage::~Storage()
    {}

    /*--------.
    | Storage |
    `--------*/

    bool
    Storage::exist(Address const& address,
                   nucleus::proton::Revision const& revision) const
    {
      ELLE_TRACE_METHOD(address, revision);

      ELLE_ASSERT(address.network() == this->_network);

      return this->_exist(this->_identifier(address, revision));
    }

    std::string
    Storage::_identifier(Address const& address,
                         nucleus::proton::Revision const& revision) const
    {
      if (revision == nucleus::proton::Revision::Last)
        return address.unique();
      else
        {
          static boost::format format("%s#%s");
          return str(boost::format(format)
                     % address.unique()
                     % revision.number);
        }
    }

    void
    Storage::store(nucleus::proton::Address const& address,
                   nucleus::proton::Block const& block)
    {
      switch (address.family())
      {
        case nucleus::proton::Family::content_hash_block:
        {
          nucleus::proton::ImmutableBlock const* ib;

          ELLE_ASSERT_NEQ(
            dynamic_cast<const nucleus::proton::ImmutableBlock*>(&block),
            nullptr);
          ib = static_cast<const nucleus::proton::ImmutableBlock*>(&block);

          this->store(address, *ib);

          break;
        }
        case nucleus::proton::Family::public_key_block:
        case nucleus::proton::Family::owner_key_block:
        case nucleus::proton::Family::imprint_block:
        {
          nucleus::proton::MutableBlock const* mb;

          ELLE_ASSERT_NEQ(
            dynamic_cast<const nucleus::proton::MutableBlock*>(&block),
            nullptr);
          mb = static_cast<const nucleus::proton::MutableBlock*>(&block);

          this->store(address, *mb);

          break;
        }
      default:
        throw Exception(
          elle::sprintf("unknown block family '%u'", address.family()));
      }
    }

    void
    Storage::store(const nucleus::proton::Address& address,
                   const nucleus::proton::ImmutableBlock& block)
    {
      ELLE_TRACE_METHOD(address, block);

      ELLE_ASSERT(address.network() == this->_network);

      if (this->_exist(this->_identifier(address)))
        throw Exception("this block seems to already exists.");

      this->_store(address, block);
    }

    void
    Storage::store(const nucleus::proton::Address& address,
                   const nucleus::proton::MutableBlock& block)
    {
      ELLE_TRACE_METHOD(address, block);

      ELLE_ASSERT(address.network() == this->_network);

      if (conflict(address, block))
        throw Exception("the block does not derive the local one.");

      this->_store(address, block);
    }

    std::unique_ptr<nucleus::proton::Block>
    Storage::load(nucleus::proton::Address const& address) const
    {
      ELLE_TRACE_METHOD(address);

      ELLE_ASSERT(address.network() == this->_network);

      if (!this->_exist(this->_identifier(address)))
        throw Exception("the block does not seem to exist.");

      return this->_load(address);
    }

    std::unique_ptr<nucleus::proton::Block>
    Storage::load(nucleus::proton::Address const& address,
                  nucleus::proton::Revision const& revision) const
    {
      ELLE_TRACE_METHOD(address, revision);

      ELLE_ASSERT(address.network() == this->_network);

      // Since block history is not supported yet...
      ELLE_ASSERT(revision == nucleus::proton::Revision::Last);

      if (!this->_exist(this->_identifier(address, revision)))
        throw Exception("the block does not seem to exist.");

      return this->_load(address, revision);
    }

    void
    Storage::erase(nucleus::proton::Address const& address)
    {
      ELLE_TRACE_METHOD(address);

      ELLE_ASSERT(address.network() == this->_network);

      // Check block existence before trying to delete it.
      if(!this->_exist(this->_identifier(address)))
        throw Exception("the block you tried to erase doesn't exist.");

      this->_erase(address);
    }


    /*----------.
    | Printable |
    `----------*/

    void
    Storage::print(std::ostream& stream) const
    {
      stream << "Storage(" << this->network() << ")";
    }

    /*----------.
    | Utilities |
    `----------*/

    bool
    Storage::conflict(nucleus::proton::Address const& address,
                      nucleus::proton::MutableBlock const& block) const
    {
      // If the block doesn't exist locally, this is the first version
      // we know of and there is no conflict.
      if (!this->_exist(this->_identifier(address)))
        {
          ELLE_DEBUG("this is the first revision of the block.");
          return false;
        }

      ELLE_DEBUG_SCOPE("the mutable block seems to exist "
                       "locally: make sure it derives the "
                       "current revision");

      std::unique_ptr<nucleus::proton::MutableBlock> current =
        elle::cast<nucleus::proton::MutableBlock>::runtime(
          load(address));

      ELLE_DEBUG("current block revision '%s' and given block "
                 "revision is '%s'.",
                 current->revision(), block.revision());

      // Check if the block derives from the local block.
      if (block.revision() != current->revision())
        return !block.derives(*current);

      ELLE_DEBUG("block have same revision, we need to distinguish them "
                 "as the latest.");

      // We check if contents are the same.
      {
        elle::Buffer b1;
        b1.writer() << block;

        elle::Buffer b2;
        b2.writer() << *current;

        /// We need to choose arbitrarly one block as the latest. For that we
        /// decide to compare their content, so we got the same result on
        /// every hosts.
        if (b1 == b2)
          {
            ELLE_TRACE("both block contents match. Let's say the remote one "
                       "doesn't derive the local one");
            return false;
          }

        ELLE_WARN("conflict detected: blocks have the same revision but "
                  "different content; assuming the latest revision is '%s'.",
                  (b1 < b2 ? block : *current));

        return (b1 < b2);
      }
    }
  }
}
