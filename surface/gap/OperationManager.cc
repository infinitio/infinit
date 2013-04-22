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

    OperationManager::OperationId
    OperationManager::_create(std::string const& name,
                              std::function<void(void)> const& cb,
                              bool auto_delete)
    {
      ELLE_TRACE("Adding operation %s.", name);
      static OperationId id = 0;
      id += 1;
      _operations[id].reset(new Operation{name, cb});
      if (auto_delete)
        _operations[id]->delete_later();
      return id;
    }

    void
    OperationManager::_cancel(std::string const& name)
    {
      for (auto& pair: _operations)
      {
        if (pair.second != nullptr && pair.second->name() == name)
        {
          pair.second->cancel();
          return;
        }
      }
    }

    void
    OperationManager::_cancel_all(std::string const& name)
    {
      for (auto& pair: _operations)
      {
        if (pair.second != nullptr &&
            boost::algorithm::ends_with(pair.second->name(), name))
        {
          pair.second->cancel();
          return;
        }
      }
    }
  }
}
