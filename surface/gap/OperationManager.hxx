#ifndef SURFACE_GAP_OPERATIONMANAGER_HXX
# define SURFACE_GAP_OPERATIONMANAGER_HXX

# include "OperationManager.hh"

# include <thread>

namespace surface
{
  namespace gap
  {
     template <typename Op, typename... Args>
     OperationManager::OperationId
     OperationManager::_add(Args&&... args)
     {
       auto id = _next_operation_id();

       this->_operations[id].reset(
         new OperationAdaptor<Op>{std::forward<Args>(args)...});

       return id;
     }

     template <typename T>
     struct OperationManager::OperationAdaptor: public T
     {
       static_assert(
           std::is_base_of<Operation, T>::value,
           "T must inherit Operation");

     public:
       typedef std::function<void ()> Callback;

     protected:
       ELLE_ATTRIBUTE(Callback, on_success_callback);
       ELLE_ATTRIBUTE(Callback, on_error_callback);
       ELLE_ATTRIBUTE(std::thread, thread);

     public:
       template <typename... Args>
       OperationAdaptor(Args&&... args):
         T(std::forward<Args>(args)...),
         _on_success_callback{
           std::bind(&OperationAdaptor<T>::_on_success, this)},
         _on_error_callback{
           std::bind(&OperationAdaptor<T>::_on_error, this)},
         _thread{std::bind(&OperationAdaptor<T>::_start, this)}
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
           ELLE_ERR("LOGIC ERROR: %s", this->_exception_string());
         }

         try
         {
           if (!this->_done && !this->_cancelled)
             this->cancel();

           if (this->_thread.joinable())
           {
             ELLE_DEBUG("wait operation %s to finish", this->_name)
               this->_thread.join();
             ELLE_DEBUG("successfully joined");
           }
           else
           {
             ELLE_DEBUG("not joinable");
           }
         }
         catch (...)
         {
           ELLE_ERR("couldn't join the operation's thread of %s: %s",
                    this->_exception_string(),
                    this->_name);
         }
       }

     private:
       void
       _start()
       {
         ELLE_LOG_COMPONENT("infinit.surface.gap.Operation");
         ELLE_TRACE_FUNCTION(this->_name);

         try
         {
           this->_succeeded = false;
           ELLE_TRACE("Running long operation: %s", this->_name);
           this->_run();
           this->_succeeded = true;
         }
         catch (...)
         {
           this->_exception = std::current_exception();
         }

         if (!this->_succeeded)
         {
           ELLE_ERR("operation %s threw an exception: %s (not rethrown yet)",
                    this->_name,
                    this->failure_reason());
         }

         this->_done = true;

         if (this->_cancelled)
         {
           ELLE_DEBUG("operation %s has been cancelled", this->_name);
           return;
         }

         try
         {
           if (this->_succeeded && this->_on_success_callback)
             this->_on_success_callback();
           else if (!this->_succeeded && this->_on_error_callback)
             this->_on_error_callback();
         }
         catch (...)
         {
           ELLE_ERR("%s handler for operation %s failed: %s",
                    this->_succeeded ? "success" : "error",
                    this->_name,
                    this->_exception_string());
         }
       }
     };
  }
}

#endif
