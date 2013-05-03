#include "../State.hh"
#include "Operation.hh"

#include <boost/algorithm/string/predicate.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {

    State::OperationStatus
    State::operation_status(OperationId const id) const
    {
      ELLE_TRACE_METHOD(id);
      auto it = _operations.find(id);
      if (it == _operations.end())
        throw elle::Exception{
            "Couldn't find any operation with id " + std::to_string(id)
        };
      if (!it->second->done())
        return OperationStatus::running;
      // the operation is terminated.
      if (it->second->succeeded())
        return OperationStatus::success;
      return OperationStatus::failure;
    }

    void
    State::operation_finalize(OperationId const id)
    {
      ELLE_TRACE_METHOD(id);
      auto it = _operations.find(id);
      if (it == _operations.end())
        throw elle::Exception{
            "Couldn't find any operation with id " + std::to_string(id)
        };
      if (!it->second->done())
        throw elle::Exception{"Operation not finished"};
      if (!it->second->succeeded())
        it->second->rethrow();
    }

    State::OperationId
    State::_next_operation_id()
    {
      static State::OperationId id = 0;
      return ++id;
    }

    void
    State::_cancel_operation(std::string const& name)
    {
      ELLE_TRACE_METHOD(name);
      for (auto& pair: _operations)
      {
        if (pair.second != nullptr && pair.second->name() == name && !pair.second->done())
        {
          pair.second->cancel();
          return;
        }
      }
    }

    void
    State::_cancel_all_operations(std::string const& name)
    {
      ELLE_TRACE_METHOD(name);
      for (auto& pair: _operations)
      {
        if (pair.second != nullptr &&
            !pair.second->done() &&
            boost::algorithm::ends_with(pair.second->name(), name))
        {
          pair.second->cancel();
          return;
        }
      }
    }

    void
    State::_cancel_all_operations()
    {
      ELLE_TRACE_METHOD("");
      for (auto& pair: _operations)
      {
        if (pair.second != nullptr &&
            !pair.second->done())
        {
          pair.second->cancel();
          return;
        }
      }
    }

  }
}
