#include "OperationManager.hh"

#include <boost/algorithm/string/predicate.hpp>

#include <elle/Exception.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {

    OperationManager::OperationStatus
    OperationManager::status(OperationId const id) const
    {
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

    OperationManager::~OperationManager()
    {
      for (auto& operation: this->_operations)
        operation.second->cancel();
    }

    OperationManager::OperationId
    OperationManager::_next_operation_id()
    {
      static OperationManager::OperationId id = 0;
      return ++id;
    }

    void
    OperationManager::finalize(OperationId const id)
    {
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

    void
    OperationManager::_cancel(std::string const& name)
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
    OperationManager::_cancel_all(std::string const& name)
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
    OperationManager::_cancel_all()
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
