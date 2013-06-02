#ifndef SURFACE_GAP_STATE_OPERATION_HH
# define SURFACE_GAP_STATE_OPERATION_HH

# include "Operation.hh"

# include <elle/log.hh>
# include <elle/Exception.hh>

# include <functional>

namespace surface
{
  namespace gap
  {
    /// Base class for specialized operation managers.
    class OperationManager
    {
    private:
      // Glue to manage thread creation and destruction.
      template <typename T>
      struct OperationAdaptor;

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

      /// Clean all terminated operations.
      void
      cleanup();

    protected:
     static
      OperationId
     _next_operation_id();

     /// Fire and store a new operation of type `Op` constructed with `args`.
     template <typename Op, typename... Args>
     OperationId
     _add(Args&&... args);

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

    class OperationManager::Operation
    {
    protected:
      std::string _name;
      bool _done;
      bool _succeeded;
      bool _cancelled;
      bool _delete_later;
      bool _rethrown;
      std::exception_ptr _exception;
      std::string _failure_reason;

    public:
      Operation(std::string const& name);
      virtual
      ~Operation();

     public:
       std::string const& name() const { return this->_name; }
       bool done() const { return this->_done; }
       bool succeeded() const { return this->_succeeded; }
       bool cancelled() const { return this->_cancelled; }
       bool scheduled_for_deletion() const { return this->_delete_later; }
       void delete_later(bool flag = true) { this->_delete_later = flag; }

     protected:
       /// Body of the operation. If the operation is cancellable, a check of
       /// the protected member _cancelled should be done as often as possible.
       virtual
       void
       _run() = 0;

       //- Cancelletion -------------------------------------------------------
     public:
       /// Cancel the transaction by setting _cancelled to true. The inherited
       /// class has to implement other cancelletion behaviors by overriding
       /// the method `void _cancel()`.
       void
       cancel();

     protected:
       /// Allow user to create special behavior when the operation is
       /// cancelled. The body of this function is deliberately empty.
       virtual
       void
       _cancel();

       //- Deletion -------------------------------------------------------------
     public:
       /// This method is used to notify that the process is scheduled for
       /// deletion.

       /// Rethrow the catched exception that may occured during _run.
       void
       rethrow();

     protected:
        // XXX put this in elle ?
        /// Try to obtain a string from the exception pointer e or the current
        /// exception when none is given.
        std::string
        _exception_string(std::exception_ptr eptr = std::exception_ptr{});

     public:
        /// When succeeded is false, returns the stored exception string.
        std::string
        failure_reason();
     };
  }
}

# include "OperationManager.hxx"

#endif
