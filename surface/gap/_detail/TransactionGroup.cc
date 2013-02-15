#include <elle/log.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/HexadecimalArchive.hh>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/flat_set.hpp>

#include <surface/gap/_detail/TransactionGroup.hh>
#include <surface/gap/State.hh>

#include <utility>
#include <memory>
#include <functional>

ELLE_LOG_COMPONENT("surface.gap.TransactionGroup");

#include "impl.hh"

namespace surface {
namespace gap {

using HexArchive = elle::serialize::InputHexadecimalArchive;
using elle::serialize::from_string;

struct TransactionGroup::Impl
{
  using key_type = unsigned long;

  Impl(SharedStateManager &shm)
    //: _transactions_mutex{}
    : _transactions{std::less<key_type const>{}, shm.get_segment_manager()}
    , _transactions_dirty{true}
  {
  }

public:
  //ipc::interprocess_sharable_mutex mutable        _transactions_mutex;
  shared_map<key_type, SharedTransactionPtr>      _transactions;
  bool                                            _transactions_dirty;
};

TransactionGroup::TransactionGroup(SharedStateManager &shm)
  : self{nullptr}
  , _shm(shm)
{
  self = shm.find_or_construct<TransactionGroup::Impl>
    ("TransactionGroup::Impl")(shm);
}

TransactionGroup::~TransactionGroup()
{
}

void
TransactionGroup::update(std::string const &id, int status)
{
  unsigned long index;

  from_string<HexArchive>(id) >> index;
  auto it = self->_transactions.find(index);
  if (it == self->_transactions.end())
    {
      throw Exception{
          gap_error,
          "TransactionGroup::update, try yo update a non-existing transaction"
      };
    }
  auto &t = it->second;

  t->_status = status;
}

void
TransactionGroup::update(std::string const &id,
                         Transaction const &updated)
{
  unsigned long index;

  from_string<HexArchive>(id) >> index;
  auto it = self->_transactions.find(index);
  if (it == self->_transactions.end())
    {
      throw Exception{
          gap_error,
          "TransactionGroup::update, try yo update a non-existing transaction"
      };
    }
  auto &t = it->second;

  *t = updated;
}
void
TransactionGroup::add(Transaction const &t)
{
  unsigned long index;
  SharedTransactionPtr ptr{new SharedTransaction{t, _shm}};

  from_string<HexArchive>(t.transaction_id) >> index;
  self->_transactions[index] = std::move(ptr);
}

void
TransactionGroup::del(std::string const &tid)
{
  unsigned long index;

  from_string<HexArchive>(tid) >> index;
  auto it = self->_transactions.find(index);
  if (it == self->_transactions.end())
    {
      return ;
    }
  self->_transactions.erase(it);
}

boost::optional<Transaction>
TransactionGroup::get(std::string const &tid)
{
  unsigned long index;

  from_string<HexArchive>(tid) >> index;
  auto it = self->_transactions.find(index);
  if (it == self->_transactions.end())
   {
     // break program.
     return boost::none_t{};
   }
  auto &t = it->second;

  return static_cast<Transaction>(*t);
}

} /* gap */
} /* surface */
