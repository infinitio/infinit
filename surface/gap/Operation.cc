#include "Operation.hh"

#include <elle/log.hh>
#include <elle/Exception.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");

namespace surface
{
  namespace gap
  {
    Operation::Operation(std::string const& name):
      _name{name},
      _done{false},
      _succeeded{false},
      _cancelled{false},
      _delete_later{false},
      _rethrown{false},
      _exception{},
      _failure_reason{}
    {
      ELLE_TRACE_METHOD("");
    }

    Operation::~Operation()
    {
      ELLE_TRACE_METHOD("");
    }

    void
    Operation::cancel()
    {
      ELLE_TRACE_METHOD("");

      ELLE_TRACE("cancelling operations '%s'", this->_name);

      if (this->_done || this->_cancelled)
        return;
      this->_cancelled = true;
      this->_done = true;
      this->_cancel();
    }

    void
    Operation::_on_error()
    {
      ELLE_DEBUG_METHOD("");

      ELLE_DEBUG("handling error for operation '%s'", this->_name);
    }

    void
    Operation::_on_success()
    {
      ELLE_DEBUG_METHOD("");

      ELLE_DEBUG("handling success for operation '%s'", this->_name);
    }

    void
    Operation::_cancel()
    {
      ELLE_DEBUG_METHOD("");

      ELLE_DEBUG("handling cancel for operation '%s'", this->_name);
    }

    void
    Operation::rethrow()
    {
      ELLE_TRACE_METHOD("");

      ELLE_DEBUG("handling rethrow for operation '%s'", this->_name);

      this->_rethrown = true;
      std::rethrow_exception(this->_exception);
    }

    std::string
    Operation::failure_reason()
    {
      ELLE_TRACE_METHOD("");

      if (this->_exception == std::exception_ptr{})
        throw elle::Exception{"no current exception"};

      if (this->_failure_reason.empty())
        this->_failure_reason = elle::exception_string(this->_exception);

      return this->_failure_reason;
    }
  }
}
