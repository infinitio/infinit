#include "Operation.hh"

#include <elle/log.hh>
#include <elle/Exception.hh>

ELLE_LOG_COMPONENT("surface.gap.Operation");

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
    {}

    Operation::~Operation()
    {}

    void
    Operation::cancel()
    {
      ELLE_TRACE_METHOD(this->_name);
      if (this->_done || this->_cancelled)
        return;
      this->_cancelled = true;
      this->_cancel();
    }

    void
    Operation::_cancel()
    {
      ELLE_TRACE_FUNCTION(this->_name);
    }

    void
    Operation::rethrow()
    {
      ELLE_TRACE_FUNCTION(this->_name);

      this->_rethrown = true;
      std::rethrow_exception(this->_exception);
    }

    std::string
    Operation::_exception_string(std::exception_ptr eptr)
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
    Operation::failure_reason()
    {
      if (!this->_exception)
        throw elle::Exception{"no current exception"};

      if (this->_failure_reason.empty())
        this->_failure_reason = elle::exception_string(this->_exception);

      return this->_failure_reason;
    }
  }
}
