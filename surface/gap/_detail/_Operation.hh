#ifndef  SURFACE_GAP_STATE_OPERATION_HH
# define SURFACE_GAP_STATE_OPERATION_HH

# include "../State.hh"

# include <elle/log.hh>

# include <functional>
# include <stdexcept>
# include <string>
# include <thread>

namespace surface
{
  namespace gap
  {

    struct State::Operation
    {
    public:
      typedef std::function<void(void)> Callback;

    private:
      Callback            _callback;
      std::string         _name;
      bool                _done;
      bool                _success;
      bool                _cancelled;
      bool                _delete_later;
      std::exception_ptr  _exception;
      std::thread         _thread;

    public:
      Operation(std::string const& name,
                Callback const& cb)
        : _callback{cb}
        , _name{name}
        , _done{false}
        , _success{false}
        , _cancelled{false}
        , _delete_later{false}
        , _exception{}
        , _thread{&Operation::_run, this}
      {
        ELLE_LOG_COMPONENT("infinit.surface.gap.State");
        ELLE_TRACE("Creating long operation: %s", this->_name);
      }

      virtual
      ~Operation()
      {
        ELLE_LOG_COMPONENT("infinit.surface.gap.State");
        ELLE_LOG("Destroying long operation: %s", this->_name);

        try
        {
          if (this->_thread.joinable())
            this->_thread.join();
        }
        catch (...)
        {
          ELLE_ERR("Couldn't join the operation's thread of %s", _name);
        }
      }

      std::string const&
      name() const { return _name; }

      virtual
      Operation& cancel()
      {
        this->_cancelled = true;
        return *this;
      }
      bool cancelled() const { return _cancelled; }
      bool done() const { return _done; }
      bool succeeded() const { return _success; }
      bool scheduled_for_deletion() const { return _delete_later; }

      Operation&
      delete_later(bool const flag = true)
      {
        this->_delete_later = flag;
        return *this;
      }

      void rethrow()
      {
        std::rethrow_exception(this->_exception);
      }
    private:
      void _run()
      {
        ELLE_LOG_COMPONENT("infinit.surface.gap.State");
        try
        {
          ELLE_TRACE("Running long operation: %s", this->_name);
          (this->_callback)();
          _success = true;
        }
        catch (std::runtime_error const& e)
        {
          this->_exception = std::current_exception();
          ELLE_ERR("Operation %s threw an exception: %s (not handle yet)",
                   this->_name,
                   e.what());
        }
        _done = true;
      }
    };

  }
}

#endif
