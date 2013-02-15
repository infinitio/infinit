#pragma once
#ifndef TRANSACTIONMANAGER_V6FXBWQ3
#define TRANSACTIONMANAGER_V6FXBWQ3

#include <boost/optional.hpp>
#include <plasma/plasma.hh>
#include <memory>

namespace surface {
namespace gap {

struct SharedStateManager;
using Transaction = plasma::Transaction;

class TransactionGroup
{
 public:
  TransactionGroup(SharedStateManager &shm);
  ~TransactionGroup();

  void
  update(std::string const& tid,
         int status);

  void
  update(std::string const& tid,
         Transaction const &t);

  void
  add(Transaction const& t);

  void
  del(std::string const& tid);

  boost::optional<Transaction>
  get(std::string const& tid);

 private:
  struct Impl;
  Impl*                 self;
  SharedStateManager&   _shm;
};

} // gap
} // surface

#endif /* end of include guard: TRANSACTIONMANAGER_V6FXBWQ3 */
