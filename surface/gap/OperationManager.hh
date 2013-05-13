#ifndef  SURFACE_GAP_STATE_OPERATION_HH
# define SURFACE_GAP_STATE_OPERATION_HH

# include <elle/log.hh>
# include <elle/Exception.hh>

# include <functional>
# include <stdexcept>
# include <string>
# include <thread>

namespace surface
{
  namespace gap
  {
   class OperationManager
    {
    public:
     class Operation
     {
     protected:
       std::string _name;
       bool _done;
       bool _succeeded;
       bool _cancelled;
       bool _delete_later;
       bool _rethrown;
       std::exception_ptr _exception;

     public:
       std::string const& name() const { return this->_name; }
       bool done() const { return this->_done; }
       bool succeeded() const { return this->_succeeded; }
       bool cancelled() const { return this->_cancelled; }
       bool delete_later() const { return this->_delete_later; }

       //- Cancelletion ---------------------------------------------------------
     public:
       /// This method is used to cancel the transaction by setting _cancel to
       /// true. The inherited class has to implement the cancelletion behavior
       /// on _cancel method.
       void
       cancel()
       {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_METHOD(this->_name);
         if (this->_done || this->_cancelled)
           return;
         this->_cancelled = true;
         this->_cancel();
       }

     protected:
       /// This method allow user to create special behavior when the operation
       /// is cancelled. The body of this function is deliberately empty.
       virtual
       void
       _cancel()
       {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_FUNCTION(this->_name);
       }

       //- Deletion -------------------------------------------------------------
     public:
       /// This method is used to notify that the process is scheduled for
       /// deletion.
       void
       delete_later(bool const flag = true)
        {
         this->_delete_later = flag;
        }
       /// Rethrow the catched exception that may occured during _run.
       void
       rethrow()
        {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_FUNCTION(this->_name);

         this->_rethrown = true;
         std::rethrow_exception(this->_exception);
        }

       std::string
       failure_reason()
       {
         if (!this->_exception)
           throw elle::Exception{"No current exception"};

         try
         {
           std::rethrow_exception(this->_exception);
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

     public:
       Operation(std::string const& name)
         : _name{name}
         , _done{false}
         , _succeeded{false}
         , _cancelled{false}
         , _delete_later{false}
         , _rethrown{false}
         , _exception{}
       {}

       virtual
       ~Operation() {}

     protected:
       // The doc goes here.
       virtual
       void
       _run() = 0;
     };

     template <typename T>
     struct OperationAdaptor: public T
      {
       static_assert(std::is_base_of<Operation, T>::value, "");

     public:
       typedef std::function<void (Operation &)> Callback;

     protected:
       ELLE_ATTRIBUTE(Callback, on_success_callback);
       ELLE_ATTRIBUTE(Callback, on_error_callback);
       ELLE_ATTRIBUTE(std::thread, thread);

     public:
       template <typename... Args>
       OperationAdaptor(Args&&... args)
         : T(std::forward<Args>(args)...)
         , _on_success_callback{std::bind(&OperationAdaptor<T>::_on_success, this, std::placeholders::_1)}
         , _on_error_callback{std::bind(&OperationAdaptor<T>::_on_error, this, std::placeholders::_1)}
         , _thread{std::bind(&OperationAdaptor<T>::_start, this)}
        {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_FUNCTION(this->_name);
        }

       virtual
       ~OperationAdaptor()
        {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_FUNCTION(this->_name);

         try
         {
           if (this->_done && !this->_succeeded && !this->_rethrown)
           {
             ELLE_WARN("operation %s deleted without having been checked: %s",
                       this->_name, this->failure_reason());
           }
           else if (!this->_done)
           {
             ELLE_WARN("destroying running operation %s", this->_name);
           }
         }
         catch (...)
         {
           ELLE_ERR("this is not normal...");
         }

         try
         {
           if (!this->_done && !this->_cancelled)
           {
             this->cancel();
           }
           if (this->_thread.joinable())
           {
             ELLE_DEBUG("wait operation %s to finish", this->_name);
             this->_thread.join();
           }
           else
           {
             ELLE_DEBUG("not joinable");
           }

           ELLE_DEBUG("successfully joined");
         }
         catch (std::exception const& e)
         {
           ELLE_ERR("couldn't join the operation's thread of %s: %s",
                    this->_name,
                    e.what());
         }
         catch (...)
         {
           ELLE_ERR("couldn't join the operation's thread of %s: unknown",
                    this->_name);
         }
        }

       virtual
       void
       _on_error(Operation&)
       {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_FUNCTION(this->_name);
       }

       virtual
       void
       _on_success(Operation&)
       {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_FUNCTION(this->_name);
       }

     private:
       void
       _start()
       {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_FUNCTION(this->_name);

         try
         {
           ELLE_TRACE("Running long operation: %s", this->_name);
           this->_run();
           this->_succeeded = true;
         }
         catch (std::exception const& e)
         {
           this->_exception = std::current_exception();
           ELLE_ERR("Operation %s threw an exception: %s (not rethrown yet)",
                    this->_name,
                    e.what());
         }
         catch (...)
         {
           this->_exception = std::current_exception();
           ELLE_ERR("Operation %s threw an exception: unknow (not rethrown yet)",
                    this->_name);
         }

         this->_done = true;

         if (this->_cancelled)
         {
           ELLE_DEBUG("operation has been cancelled");
           return;
         }

         try
         {
           if (this->_succeeded && this->_on_success_callback)
             this->_on_success_callback(*this);
           else if (!this->_succeeded && this->_on_error_callback)
             this->_on_error_callback(*this);
         }
         catch (std::exception const& e)
         {
           ELLE_ERR("%s handler for operation %s failed: %s",
                    this->_succeeded ? "success" : "error",
                    this->_name,
                    e.what());
         }
         catch (...)
         {
           ELLE_ERR("%s handler for operation %s failed: unknown",
                    this->_succeeded ? "success" : "error",
                    this->_name);
         }
       }
     };

    public:
      /// A process is indexed with a unique identifier.
      typedef size_t OperationId;

      /// The status of a process. failure or success implies that the process
      /// is terminated.
      enum class OperationStatus : int { failure = 0, success = 1, running = 2};

    private:
      typedef std::unique_ptr<Operation> OperationPtr;
      typedef std::unordered_map<OperationId, OperationPtr> OperationMap;
      OperationMap _operations;

    public:
      virtual
      ~OperationManager();

    public:
      /// Retreive the status of an existing operation.
      OperationStatus
      status(OperationId const id) const;

      /// @brief Remove a operation and throw the exception if any.
      ///
      /// @throw if the operation does not exist or if it is still running.
      void
      finalize(OperationId const id);

    protected:
     static
      OperationId
     _next_operation_id();

     template <typename Op, typename... Args>
     OperationId
     _add(Args&&... args)
     {
       auto id = _next_operation_id();

       this->_operations[id].reset(
         new OperationAdaptor<Op>{std::forward<Args>(args)...});

       return id;
     }

      virtual
      void
      _cancel(std::string const& name);

      virtual
      void
      _cancel_all(std::string const& name);

     virtual
     void
     _cancel_all();
    };

  }
}

#endif
