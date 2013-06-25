#include <elle/Exception.hh>
#include <elle/log.hh>

#include <hole/Hole.hh>
#include <hole/Exception.hh>
#include <hole/Passport.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/ImmutableBlock.hh>
#include <nucleus/proton/MutableBlock.hh>

#include <Infinit.hh>

#include <boost/format.hpp>

ELLE_LOG_COMPONENT("infinit.hole.Hole");

namespace hole
{
  /*-------------.
  | Construction |
  `-------------*/

  Hole::Hole(storage::Storage& storage,
             Passport const& passport,
             cryptography::PublicKey const& authority_K):
    _storage(storage),
    _passport(passport),
    _authority_K(authority_K)
  {
    if (!_passport.validate(this->_authority_K))
      throw Exception("unable to validate the passport");
  }

  Hole::~Hole()
  {}

  /*------.
  | Ready |
  `------*/

  void
  Hole::push(const nucleus::proton::Address& address,
             const nucleus::proton::Block& block)
  {
    ELLE_TRACE_SCOPE("%s(%s, %s)", __FUNCTION__, address, block);

    // Forward the request depending on the nature of the block which
    // the address indicates.
    switch (address.family())
      {
      case nucleus::proton::Family::content_hash_block:
        {
          const nucleus::proton::ImmutableBlock*        ib;
          ib = static_cast<const nucleus::proton::ImmutableBlock*>(&block);
          assert(dynamic_cast<const nucleus::proton::ImmutableBlock*>(
                   &block) != nullptr);
          this->_push(address, *ib);
          break;
        }
      case nucleus::proton::Family::public_key_block:
      case nucleus::proton::Family::owner_key_block:
      case nucleus::proton::Family::imprint_block:
        {
          const nucleus::proton::MutableBlock*          mb;
          mb = static_cast<const nucleus::proton::MutableBlock*>(&block);
          assert(dynamic_cast<const nucleus::proton::MutableBlock*>(
                   &block) != nullptr);
          this->_push(address, *mb);
          break;
        }
      default:
        {
          throw Exception(elle::sprintf("unknown block family '%u'",
                                              address.family()));
        }
      }
  }

  std::unique_ptr<nucleus::proton::Block>
  Hole::pull(const nucleus::proton::Address& address,
             const nucleus::proton::Revision& revision)
  {
    ELLE_TRACE_SCOPE("%s(%s, %s)", __FUNCTION__, address, revision);

    // Forward the request depending on the nature of the block which
    // the addres indicates.
    switch (address.family())
      {
      case nucleus::proton::Family::content_hash_block:
        return this->_pull(address);
      case nucleus::proton::Family::public_key_block:
      case nucleus::proton::Family::owner_key_block:
      case nucleus::proton::Family::imprint_block:
        return this->_pull(address, revision);
      default:
        throw Exception(elle::sprintf("unknown block family '%u'",
                                            address.family()));
      }
  }

  void
  Hole::wipe(nucleus::proton::Address const& address)
  {
    ELLE_TRACE_SCOPE("%s(%s)", __FUNCTION__, address);
    this->_wipe(address);
  }
}
