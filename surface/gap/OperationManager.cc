#include "OperationManager.hh"

#include <boost/algorithm/string/predicate.hpp>

#include <elle/Exception.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.OperationManager");

namespace surface
{
  namespace gap
  {

    //- OperationManager ------------------------------------------------------

    OperationManager::OperationStatus
    OperationManager::status(OperationId const id) const
    {
      if (id == 0)
        return OperationStatus::success;
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
    OperationManager::cancel_operation(OperationId const id)
    {
      auto it = _operations.find(id);
      if (it == _operations.end())
        throw elle::Exception{
            "Couldn't find any operation with id " + std::to_string(id)
        };
      if (!it->second->done())
        it->second->cancel();
    }

    OperationManager::~OperationManager()
    {
      for (auto& operation: this->_operations)
      {
        if (operation.second == nullptr)
          continue;
        try
        {
          operation.second->cancel();
        }
        catch (...)
        {
          ELLE_ERR("couldn't cancel operation %s: %s",
                   operation.second->name(),
                   elle::exception_string());
        }
      }
    }

    OperationManager::OperationId
    OperationManager::_next_operation_id()
    {
      static OperationManager::OperationId id = 0; // XXX not thread safe
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
    OperationManager::cleanup()
    {
      std::vector<OperationId> to_remove;
      for (auto& pair: this->_operations)
      {
        auto& ptr = pair.second;
        if ((ptr == nullptr) ||
            (ptr->done() && ptr->scheduled_for_deletion()))
          to_remove.push_back(pair.first);
      }
      for (auto id: to_remove)
        this->finalize(id);
    }

    void
    OperationManager::_cancel(std::string const& name)
    {
      ELLE_TRACE_METHOD(name);
      for (auto& pair: _operations)
      {
       if (pair.second != nullptr &&
           pair.second->name() == name &&
           !pair.second->done())
        {
          pair.second->cancel();
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
        }
      }
    }
  }
}
