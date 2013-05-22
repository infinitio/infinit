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

    //- Operation -------------------------------------------------------------

    OperationManager::Operation::Operation(std::string const& name)
      : _name{name}
      , _done{false}
      , _succeeded{false}
      , _cancelled{false}
      , _delete_later{false}
      , _rethrown{false}
      , _exception{}
      , _failure_reason{}
    {}

    OperationManager::Operation::~Operation()
    {}

    void
    OperationManager::Operation::cancel()
    {
      ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
      ELLE_TRACE_METHOD(this->_name);
      if (this->_done || this->_cancelled)
        return;
      this->_cancelled = true;
      this->_cancel();
    }

    void
    OperationManager::Operation::_cancel()
    {
      ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
      ELLE_TRACE_FUNCTION(this->_name);
    }

    void
    OperationManager::Operation::rethrow()
    {
      ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
      ELLE_TRACE_FUNCTION(this->_name);

      this->_rethrown = true;
      std::rethrow_exception(this->_exception);
    }

    std::string
    OperationManager::Operation::_exception_string(std::exception_ptr eptr)
    {
      if (!eptr)
        eptr = std::current_exception();
      if (!eptr)
        throw elle::Exception{"no current exception present"};
      try
      {
        std::rethrow_exception(eptr);
      }
      catch (elle::Exception const& e)
      {
        return elle::sprint(e);
      }
      catch (std::exception const& e)
      {
        return e.what();
      }
      catch (...)
      {
        return "unknown exception type";
      }

      elle::unreachable();
    }

    std::string
    OperationManager::Operation::failure_reason()
    {
      if (!this->_exception)
        throw elle::Exception{"no current exception"};

      if (this->_failure_reason.empty())
        this->_failure_reason = this->_exception_string(this->_exception);

      return this->_failure_reason;
    }
  }
}
